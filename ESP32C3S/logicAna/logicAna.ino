#include <stdlib.h>
#include <string.h>
// ESP32-C3 目前無法正常 Serial.print
// ESP32-C3S (C3-2M) PowerPin=19, RGBRedPin=3, RGBGreenPin=4, RGBBluePin=5
// 燒錄時請選: [開發板] -> ESP32C3 Dev Module, 然後把 [工具] -> FlashSize: "4MB" 改 "2MB"
// 22 支接腳中有 8支類比輸入接腳，每一支接腳是10位元1024分辦率。
// 接腳 A6、A7無法當成數位輸出/入接腳 !
// 接腳 A4 (SDA) 和 A5 (SCL)，支援 I2C (TWI) 通訊。

#define Max_Signals   512   // max use 512 bytes to store signals
#define LED_BUILTIN   19    // PowerPin=19
#define PINT0   2   // D2 is INT0 (for trigger)

void catch_data();  // 直接取樣 512 次
void change_INT0(char trig_mode);
void check_cmd();
void clear_ibuf();
void do_delay(int cnt);
void do_parse_cmd();
void INT0_ISR();    // INT0 中斷處理 !
void Say_Status();

#define MODE_FREE     0
#define MODE_LOW      1
#define MODE_HIGH     2
#define MODE_TOLOW    3
#define MODE_TOHIGH   4
#define MODE_CHANGE   5
const char *sTrig[6] = { "Free", "LOW", "HIGH", "To Low", "To High", "Change" };
unsigned int  cnt, dly_cnt, temp_ans;
int   n_sent, to_send;
int   busy_cnt, cnt_chars, to_work;
unsigned char   i_cnt, led_val;
char  bit_cnt, TrigMode;
char  ibuf[16];
char  xdata[Max_Signals];   // Max_Signals = 512 bytes
// --------------------------------
void setup() {
  // put your setup code here, to run once:
  pinMode(PINT0, INPUT_PULLUP);  // D2 = INT0 用此 pin 來觸發取樣
  // pinMode(3, OUTPUT);     // D3 = INT1 暫用此 pin show on/off 來測定取樣速度
  // D5 -- D8 (as bit 7..4)
  pinMode(34, INPUT);    pinMode(35, INPUT);    // pinMode(7, INPUT);    pinMode(8, INPUT);
  // A2 -- A5 (as bit 3..0)
  // pinMode(A2, INPUT);     pinMode(A3, INPUT);    pinMode(A4, INPUT);    pinMode(A5, INPUT);
//  pinMode(A4, OUTPUT);    pinMode(A3, OUTPUT);    pinMode(A2, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);   // D13 as led indicator
  Serial.begin(500000);   // 500k BAUD RATE IS OK !!
  Serial.println("-- Easy ESP32C3 Logic Analyzer by SJC --");
  Serial.println("-* Commands:");
  Serial.println("ATRST   --> reset !");
  Serial.println("ATBIT [1..8]  --> select 1 to 8 bit sampling.");
  Serial.println("ATDLY  [0..65535]  --> set delay counter from 1 to 65536.");
  Serial.println("ATFREE  --> free reading, no trigger !");
  Serial.println("ATHI    --> trigger = HI");
  Serial.println("ATLOW   --> trigger = LOW");
  Serial.println("ATTOHI  --> trigger on LOW->HI");
  Serial.println("ATTOLOW --> trigger on HI->LOW");
  Serial.println("ATCHANGE--> trigger on any change");
  Serial.println("ATPAUSE --> pause works");
  Serial.println("ATRESUME--> resume to work\n");
  clear_ibuf();   to_send = 0;  busy_cnt = 0;
  TrigMode = 0;   // 隨意讀取 free reading, no trigger
  dly_cnt = 25;   // delay counter = 0
  bit_cnt = 8;
  Say_Status();
  to_work = 1;    // enable working
  cnt = 0;        i_cnt = 0;  n_sent = 0;
}
// --------------------------------
void loop() { 
// 實測 dly_cnt = 100 時, 完整脈波 A4=432us, A3=3520us, A2=13.8ms)
// 實測 dly_cnt = 50 時, 完整脈波 A4=116us, A3=1840us, A2=7.6ms)
// 實測 dly_cnt = 20 時, 完整脈波 A4=60us, A3=940us, A2=3.8ms)
// 實測 dly_cnt = 10 時, 完整脈波 A4=38us, A3=620us, A2=2560us)
// 實測 dly_cnt = 8 時, 完整脈波 A4=34us, A3=560us, A2=2266us)
// 實測 dly_cnt = 1 時, 完整脈波 A4=測不準4us, A3=344us, A2=1440us)
//  Serial.print("_");
  cnt ++;     // Serial.println(cnt);
  if (cnt > 500) {
    cnt = 0;      
    led_val ++;
    if (led_val > 1) led_val = 0;
    digitalWrite(LED_BUILTIN, led_val);
  }
  // digitalWrite(LED_BUILTIN, HIGH);
//  digitalWrite(A4, HIGH);   // 實測 1 Hi = 216 us (*2 = 432 us)
  i_cnt ++;
//  if ((i_cnt & 0x07) == 0) digitalWrite(A3, (i_cnt >> 3) & 1);  // ATime=0.2ms (1 Hi = 4大格*400us + 2小格*80us = 1760us)
//  if ((i_cnt & 0x1F) == 0) digitalWrite(A2, (i_cnt >> 5) & 1);  // ATime=5ms (1 Hi = 4 小格*1ms = 4ms)
  if (i_cnt > 0x40) i_cnt = 0;
  if (to_send) do_send();
  check_cmd();
  do_delay(25);
//  Serial.println("|");
  // digitalWrite(LED_BUILTIN, LOW);
//  digitalWrite(A4,LOW);
  if (to_send) do_send(); 
  check_cmd();
  do_delay(25);
}
// --------------------------------
void catch_data_1()
{ // 直接取樣 512 次
  int   n;
  char  bb;
  
  n = 0;   
  while (n < Max_Signals) {
    bb = digitalRead(34);
/*    bb = (bb << 1) | digitalRead(6);
/*    bb = (bb << 1) | digitalRead(7); 
    bb = (bb << 1) | digitalRead(8);
    bb = (bb << 1) | digitalRead(A2);
    bb = (bb << 1) | digitalRead(A3);
    bb = (bb << 1) | digitalRead(A4);
    bb = (bb << 1) | digitalRead(A5);  */
    xdata[n] = bb;    n ++;
//    do_delay(div_cnt);
  }
  to_send = 1;
}
// --------------------------------
void catch_data_2()
{ // 直接取樣 512 次
  int   n;
  char  bb;
  
  n = 0;   
  while (n < Max_Signals) {
    bb = digitalRead(35);
    bb = (bb << 1) | digitalRead(34);
    xdata[n] = bb;    n ++;
//    do_delay(div_cnt);
  }
  to_send = 1;
}
// --------------------------------
void catch_data_3()
{ // 直接取樣 512 次
  int   n;
  char  bb;
  
  n = 0;   
  while (n < Max_Signals) {
    bb = digitalRead(7);
    bb = (bb << 1) | digitalRead(6);
    bb = (bb << 1) | digitalRead(5);
    xdata[n] = bb;    n ++;
//    do_delay(div_cnt);
  }
  to_send = 1;
}
// --------------------------------
void catch_data_4()
{ // 直接取樣 512 次
  int   n;
  char  bb;
  
  n = 0;   
  while (n < Max_Signals) {
    bb = digitalRead(8);
    bb = (bb << 1) | digitalRead(7);
    bb = (bb << 1) | digitalRead(6);
    bb = (bb << 1) | digitalRead(5);
    xdata[n] = bb;    n ++;
//    do_delay(div_cnt);
  }
  to_send = 1;
}
// --------------------------------
void catch_data_5()
{ // 直接取樣 512 次
  int   n;
  char  bb;
  
  n = 0;   
  while (n < Max_Signals) {
    bb = digitalRead(A2);
    bb = (bb << 1) | digitalRead(8);
    bb = (bb << 1) | digitalRead(7);
    bb = (bb << 1) | digitalRead(6);
    bb = (bb << 1) | digitalRead(5);
    xdata[n] = bb;    n ++;
//    do_delay(div_cnt);
  }
  to_send = 1;
}
// --------------------------------
void catch_data_6()
{ // 直接取樣 512 次
  int   n;
  char  bb;
  
  n = 0;   
  while (n < Max_Signals) {
    bb = digitalRead(A3);
    bb = (bb << 1) | digitalRead(A2);
    bb = (bb << 1) | digitalRead(8);
    bb = (bb << 1) | digitalRead(7);
    bb = (bb << 1) | digitalRead(6);
    bb = (bb << 1) | digitalRead(5);
    xdata[n] = bb;    n ++;
//    do_delay(div_cnt);
  }
  to_send = 1;
}
// --------------------------------
void catch_data_7()
{ // 直接取樣 512 次
  int   n;
  char  bb;
  
  n = 0;   
  while (n < Max_Signals) {
    bb = digitalRead(A4);
    bb = (bb << 1) | digitalRead(A3);
    bb = (bb << 1) | digitalRead(A2);
    bb = (bb << 1) | digitalRead(8);
    bb = (bb << 1) | digitalRead(7);
    bb = (bb << 1) | digitalRead(6);
    bb = (bb << 1) | digitalRead(5);
    xdata[n] = bb;    n ++;
//    do_delay(div_cnt);
  }
  to_send = 1;
}
// --------------------------------
void catch_data_8()
{ // 直接取樣 512 次
  int   n;
  char  bb;
  
  n = 0;   
  while (n < Max_Signals) {
    bb = digitalRead(A5);
    bb = (bb << 1) | digitalRead(A4);
    bb = (bb << 1) | digitalRead(A3);
    bb = (bb << 1) | digitalRead(A2);
    bb = (bb << 1) | digitalRead(8);
    bb = (bb << 1) | digitalRead(7);
    bb = (bb << 1) | digitalRead(6);
    bb = (bb << 1) | digitalRead(5);
    xdata[n] = bb;    n ++;
//    do_delay(div_cnt);
  }
  to_send = 1;
}
// --------------------------------
void catch_data(void)
{ // 自動再取樣
  switch (bit_cnt) {
    case 1: catch_data_1();  break;
    case 2: catch_data_2();  break;
    case 3: catch_data_3();  break;
    case 4: catch_data_4();  break;
    case 5: catch_data_5();  break;
    case 6: catch_data_6();  break;
    case 7: catch_data_7();  break;
    case 8: catch_data_8();  break;
  }
}
// --------------------------------
void change_INT0(char trig_mode)
{
  TrigMode = trig_mode;
  switch(trig_mode) {  
    case MODE_FREE: trig_mode = CHANGE;     Serial.println("!! Trig: Free.");   break;
    case MODE_LOW: trig_mode = LOW;         Serial.println("!! Trig: Low.");    break;
    case MODE_HIGH: trig_mode = HIGH;       Serial.println("!! Trig: High.");   break;
    case MODE_TOLOW: trig_mode = FALLING;   Serial.println("!! Trig: Falling Edge.");   break;
    case MODE_TOHIGH: trig_mode = RISING;   Serial.println("!! Trig: Rising Edge.");    break;
    case MODE_CHANGE: trig_mode = CHANGE;   Serial.println("!! Trig: Change.");         break;
    default: return;   // 沒此設定 !
  }
  attachInterrupt(digitalPinToInterrupt(PINT0), INT0_ISR, trig_mode);
}
// --------------------------------
void check_cmd()
{
  char  cc;
  
  if (Serial.available() > 0) {
    cc = Serial.read();
    if (cc == 10) {  do_parse_cmd();    clear_ibuf(); }
    else {  ibuf[cnt_chars] = cc;   cnt_chars ++; }
    if (cnt_chars > 14) {
      Serial.println("!! Error: Input buffer full !!");
      clear_ibuf();
    }
  }
}
// --------------------------------
void clear_ibuf()
{
  cnt_chars = 0;    memset(ibuf, 0, 16);
}
// --------------------------------
void do_delay(int cnt_v)
{ // cnt_v = 100, 實測 6 pulses = 300 個 1-bit 取樣 
int   i, j, k;

if (cnt_v == 0) return;
k = 0;
for (i = 0;i < cnt_v; i ++) {
    for (j = 0;j < 10;j ++) k = k + i;
    temp_ans += k;
    }
}
// --------------------------------
void do_parse_cmd()
{
  if (cnt_chars < 1) return;
  Serial.print("!! Received cmd: ");
  Serial.println(ibuf);   to_work = 1;
  if ((ibuf[0] == 'A') && (ibuf[1] == 'T')) { // 開頭是 "AT"
    if (strncmp(ibuf+2, "BIT", 3) == 0)  {
        sscanf(ibuf+5, "%d", &bit_cnt);
        if (bit_cnt < 1) bit_cnt = 1;
        if (bit_cnt > 8) bit_cnt = 8;
        Serial.print("!! new sampling bit count = ");  Serial.println(bit_cnt);
        return;   }
    if (strncmp(ibuf+2, "CHANGE",6)== 0) {  change_INT0(MODE_CHANGE);      return; }
    if (strncmp(ibuf+2, "DLY", 3) == 0)  {
        sscanf(ibuf+5, "%d", &dly_cnt);
        Serial.print("!! new delay count = ");  Serial.println(dly_cnt);
        return;   }
    if (strncmp(ibuf+2, "FREE", 4) == 0) {  change_INT0(MODE_FREE);  return; }
    if (strncmp(ibuf+2, "HIGH", 4) == 0) {  change_INT0(MODE_HIGH);  return; }
    if (strncmp(ibuf+2, "LOW", 3) == 0)  {  change_INT0(MODE_LOW);   return; }
    if (strncmp(ibuf+2, "RST", 3) == 0)  {  setup();  return; }
    if (strncmp(ibuf+2, "TOHIGH",6)== 0) {  change_INT0(MODE_TOHIGH);  return; }
    if (strncmp(ibuf+2, "TOLOW", 5)== 0) {  change_INT0(MODE_TOLOW);   return; }
    if (strncmp(ibuf+2, "PAUSE", 5)== 0) {  to_work = 0;  return; } // 暫停運作
    if (strncmp(ibuf+2, "RESUME",6)== 0) {  to_work = 1;  return; } // 恢復運作
  }
}
// --------------------------------
void do_send()
{ // 把資料傳回電腦
  int   i;
  if (! to_work) return;    n_sent ++;    // if (n_sent > 50) to_work = 0;
  if (busy_cnt) {  Serial.print("!! loss ");  Serial.print(busy_cnt);  Serial.println(" records");  }
  Serial.print("*L200");
  for (i=0;i < Max_Signals;i ++) Serial.write(xdata[i]);
  Serial.println("*OK*");
  to_send = 0;    busy_cnt = 0;
  if (TrigMode == MODE_FREE) catch_data();  // 自動再取樣
}
// --------------------------------
void INT0_ISR()
{ // INT0 中斷處理 !
  if (! to_work) return;
  if (to_send) {  busy_cnt ++;  return;  }  // 損失一次資料紀錄
  catch_data();  // 自動再取樣
}
// --------------------------------
void Say_Status()
{
  int  i;
  Serial.print("!! Trigger mode: ");
  Serial.println(sTrig[TrigMode]);
  Serial.print("!! Delay counts = ");
  Serial.print(dly_cnt);    Serial.println(" counts");
  Serial.print("!! Sampling bit counts = ");
  i = bit_cnt;    Serial.println(i);
}
