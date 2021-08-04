#include <stdlib.h>
#include <string.h>
#include <task.h>   // for void taskENTER_CRITICAL(void);  void taskEXIT_CRITICAL(void);
// AmebaD  PowerPin=?? 請查 g_APinDescription[] ()
// 燒錄時請選: [開發板] -> RTL8722DM/RTL8722CSM, (VID: 0403, PID: 6001)
// LED D7=10 or 11 (閃一下即滅),
// 接腳 ?? 無法當成數位輸出/入接腳 !
// 接腳 ??，支援 I2C (TWI) 通訊。

#define Max_Signals   512   // max use 512 bytes to store signals
#define Max_7bits     0x250 // 512 bytes (8-bit) -> (7-bit) data will use 0x24A bytes (ESP32 等高速傳輸才需要 !)
// C:\Users\JurngChenSu\AppData\Local\Arduino15\packages\realtek\hardware\AmebaD\3.0.9-build20210708\variants\rtl8721d\variant.h (line 39)  or Variant.cpp (line 31-62)
#undef  LED_BUILTIN
#define LED_BUILTIN   13   // B20
#define PINT0   12    // B19 is INT0 (for trigger)
#define X7      8     // B22
#define X6      6     // B29
#define X5      5     // B28
#define X4      4     // B30
#define X3      3     // B31
#define X2      2     // B3
#define X1      1     // B1
#define X0      0     // PB_2

void catch_data();  // 直接取樣 512 次
void change_INT0(char trig_mode);
void check_cmd();
void clear_ibuf();
void do_delay(int cnt);
void do_parse_cmd();
void INT_ISR(uint32_t id, uint32_t event);    // INT 中斷處理 ! (for Ameba !!)
void Say_Status();

#define MODE_FREE     0
#define MODE_LOW      1
#define MODE_HIGH     2
#define MODE_TOLOW    3
#define MODE_TOHIGH   4
#define MODE_CHANGE   5
const char *sTrig[6] = { "Free", "LOW", "HIGH", "To Low", "To High", "Change" };
unsigned int  cnt, div_cnt, dly_cnt, temp_ans;
int   n_sent, to_send;
int   busy_cnt, cnt_chars, to_work;
unsigned char   i_cnt, led_val;
char  bit_cnt, TrigMode;
char  sCRLFs[12];
char  ibuf[64];
char  xdata[Max_Signals];   // Max_Signals = 512 bytes
char  xdata7[Max_7bits];    // 512 bytes -> 7 bit data will use 0x24A bytes
// --------------------------------
void setup() {
  int   i;
  // put your setup code here, to run once:
  pinMode(PINT0, INPUT_PULLUP);  // D2 = INT0 用此 pin 來觸發取樣
  // pinMode(3, OUTPUT);     // D3 = INT1 暫用此 pin show on/off 來測定取樣速度
  // [X0..X7] as bit [7..0]   X0 一直往高位元旋轉
  pinMode(X4, INPUT);    pinMode(X5, INPUT);    pinMode(X6, INPUT);    pinMode(X7, INPUT);
  pinMode(X0, INPUT);    pinMode(X1, INPUT);    pinMode(X2, INPUT);    pinMode(X3, INPUT);
//  pinMode(X4, INPUT_PULLUP);    pinMode(X5, INPUT_PULLUP);    pinMode(X6, INPUT_PULLUP);    pinMode(X7, INPUT_PULLUP);
//  pinMode(X0, INPUT_PULLUP);    pinMode(X1, INPUT_PULLUP);    pinMode(X2, INPUT_PULLUP);    pinMode(X3, INPUT_PULLUP);
    for (i=0;i < 10;i += 2) { sCRLFs[i] = 13;  sCRLFs[i+1] = 10; }
  sCRLFs[8] = 0;
  pinMode(LED_BUILTIN, OUTPUT);   // as led indicator  LED_BUILTIN
  Serial.begin(500000);   // 500k BAUD RATE IS OK !!
  Serial.println("-- GOOD DAY --");
  Serial.println("-- Easy AmebaD RTL8722 Logic Analyzer by SJC --");
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
  dly_cnt = 100;  // delay counter = 0
  bit_cnt = 8;    div_cnt = 1;
  Say_Status();
  to_work = 1;    // enable working
  cnt = 0;        i_cnt = 0;  n_sent = 0;
  TrigMode = MODE_TOLOW;      
  change_INT0(MODE_TOLOW);
}
// --------------------------------
void loop() { 
// 實測 dly_cnt = 100 時, 完整脈波 A4=432us, A3=3520us, A2=13.8ms)
// 實測 dly_cnt = 50 時, 完整脈波 A4=116us, A3=1840us, A2=7.6ms)
// 實測 dly_cnt = 20 時, 完整脈波 A4=60us, A3=940us, A2=3.8ms)
// 實測 dly_cnt = 10 時, 完整脈波 A4=38us, A3=620us, A2=2560us)
// 實測 dly_cnt = 8 時, 完整脈波 A4=34us, A3=560us, A2=2266us)
// 實測 dly_cnt = 1 時, 完整脈波 A4=測不準4us, A3=344us, A2=1440us)
  // Serial.print("_");
  cnt ++;     // Serial.println(cnt);
  if (cnt > 500) {
    cnt = 0;      
    led_val ++;
    if (led_val > 1) led_val = 0;
    digitalWrite(LED_BUILTIN, led_val);
    // Serial.print(led_val);
  }
  i_cnt ++;
  if (i_cnt > 0x40) i_cnt = 0;
  if (to_send) do_send();
  check_cmd();
  do_delay(dly_cnt);
  if (to_send) do_send(); 
  check_cmd();
  do_delay(dly_cnt);
}
// --------------------------------
void catch_data_1()
{ // 直接取樣 512 次
  int   n;
  char  bb;
  
  n = 0;   
  while (n < Max_Signals) {
    bb = digitalRead(X0);
/*    bb = (bb << 1) | digitalRead(X1);
    bb = (bb << 1) | digitalRead(X2); 
    bb = (bb << 1) | digitalRead(X3);
    bb = (bb << 1) | digitalRead(X4);
    bb = (bb << 1) | digitalRead(X5);
    bb = (bb << 1) | digitalRead(X6);
    bb = (bb << 1) | digitalRead(X7);  */
    xdata[n] = bb;    n ++;
    do_delay(div_cnt);
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
    bb = digitalRead(X0);
    bb = (bb << 1) | digitalRead(X1);
    xdata[n] = bb;    n ++;
    do_delay(div_cnt);
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
    bb = digitalRead(X0);
    bb = (bb << 1) | digitalRead(X1);
    bb = (bb << 1) | digitalRead(X2);
    xdata[n] = bb;    n ++;
    do_delay(div_cnt);
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
    bb = digitalRead(X0);
    bb = (bb << 1) | digitalRead(X1);
    bb = (bb << 1) | digitalRead(X2);
    bb = (bb << 1) | digitalRead(X3);
    xdata[n] = bb;    n ++;
    do_delay(div_cnt);
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
    bb = digitalRead(X0);
    bb = (bb << 1) | digitalRead(X1);
    bb = (bb << 1) | digitalRead(X2);
    bb = (bb << 1) | digitalRead(X3);
    bb = (bb << 1) | digitalRead(X4);
    xdata[n] = bb;    n ++;
    do_delay(div_cnt);
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
    bb = digitalRead(X0);
    bb = (bb << 1) | digitalRead(X1);
    bb = (bb << 1) | digitalRead(X2);
    bb = (bb << 1) | digitalRead(X3);
    bb = (bb << 1) | digitalRead(X4);
    bb = (bb << 1) | digitalRead(X5);
    xdata[n] = bb;    n ++;
    do_delay(div_cnt);
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
    bb = digitalRead(X0);
    bb = (bb << 1) | digitalRead(X1);
    bb = (bb << 1) | digitalRead(X2);
    bb = (bb << 1) | digitalRead(X3);
    bb = (bb << 1) | digitalRead(X4);
    bb = (bb << 1) | digitalRead(X5);
    bb = (bb << 1) | digitalRead(X6);
    xdata[n] = bb;    n ++;
    do_delay(div_cnt);
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
    bb = digitalRead(X0);
    bb = (bb << 1) | digitalRead(X1);
    bb = (bb << 1) | digitalRead(X2);
    bb = (bb << 1) | digitalRead(X3);
    bb = (bb << 1) | digitalRead(X4);
    bb = (bb << 1) | digitalRead(X5);
    bb = (bb << 1) | digitalRead(X6);
    bb = (bb << 1) | digitalRead(X7);
    xdata[n] = bb;    n ++;
    do_delay(div_cnt);
  }
  to_send = 1;
}
// --------------------------------
void catch_data(void)
{ // 自動再取樣
//  taskENTER_CRITICAL();
//  taskDISABLE_INTERRUPTS();
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
//  taskENABLE_INTERRUPTS();
//  taskEXIT_CRITICAL();
}
// --------------------------------
void change_INT0(char trig_mode)
{ // pinMode 參考 https://github.com/pepe2k/realtek-ameba-arduino/blob/master/hardware/cores/arduino/wiring_digital.c
  TrigMode = trig_mode;
  switch(trig_mode) {  
    case MODE_FREE: trig_mode = CHANGE;     Serial.println("!! Trig: Free.");   break;
    case MODE_LOW: trig_mode = INPUT_IRQ_LOW;       Serial.println("!! Trig: Low.");    break;
    case MODE_HIGH: trig_mode = INPUT_IRQ_HIGH;     Serial.println("!! Trig: High.");   break;
    case MODE_TOLOW: trig_mode = INPUT_IRQ_FALL;    Serial.println("!! Trig: Falling Edge.");   break;
    case MODE_TOHIGH: trig_mode = INPUT_IRQ_RISE;   Serial.println("!! Trig: Rising Edge.");    break;
    case MODE_CHANGE: trig_mode = CHANGE;   Serial.println("!! Trig: Change.");         break;
    default: return;   // 沒此設定 !
  }
  Serial.println(sCRLFs);     // 減少電腦側訊息接收失誤 (PC 端不知為何每次必須讀滿 4 字元)
//  attachInterrupt(digitalPinToInterrupt(PINT0), INT_ISR, trig_mode);
  pinMode(PINT0, trig_mode);         // !!** For Ameba **!!
  digitalSetIrqHandler(PINT0, INT_ISR);  // !!** For Ameba **!!
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
int cvt_8_to_7bit(void)
{ // convert 8-bit data to 7-bit (all b7=1, low bit 優先讀取)
char *pi, *po;
int  i, n;
char cc, nn;

pi = xdata;   po = xdata7;    cc = 0;   nn = 0;   n = 0;
for (i=0;i < Max_Signals;i ++) {
    *po = (*pi & 0x7f) | 0x80;    po ++;    // 7-th bit always set to 1
    cc = cc >> 1;     n ++; 
    if (*pi & 0x80) cc = cc | 0x40;     // save 7-th bit to another byte
    pi ++;      nn ++;
    if (nn > 6) { //
        *po = cc | 0x80;    po ++;      // write another byte, still keep 7-th bit always set to 1
        nn = 0;   n ++;     cc = 0;
        }
    }
if (nn) { *po = cc | 0x80;    n ++; }
return(n);  // 若 Max_Signals=512, 則此傳回 0x24A
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
  Serial.print("!!!!!! Received cmd: ");
  Serial.println(ibuf);   to_work = 1;
  if ((ibuf[0] == 'A') && (ibuf[1] == 'T')) { // 開頭是 "AT"
    if (strncmp(ibuf+2, "BIT", 3) == 0)  {
        sscanf(ibuf+5, "%d", &bit_cnt);
        if (bit_cnt < 1) bit_cnt = 1;
        if (bit_cnt > 8) bit_cnt = 8;
        Serial.print("!!!!!! new sampling bit count = ");  Serial.println(bit_cnt);
        Serial.println(sCRLFs);     // 減少電腦側訊息接收失誤 (PC 端不知為何每次必須讀滿 4 字元)
        return;   }
    if (strncmp(ibuf+2, "CHANGE",6)== 0) {  change_INT0(MODE_CHANGE);      return; }
    if (strncmp(ibuf+2, "DLY", 3) == 0)  {
        sscanf(ibuf+5, "%d", &div_cnt);  // dly_cnt
        Serial.print("!!!!!! new sampling delay count = ");  Serial.print(div_cnt);
        Serial.println(sCRLFs);     // 減少電腦側訊息接收失誤 (PC 端不知為何每次必須讀滿 4 字元)
        return;   }
    if (strncmp(ibuf+2, "FREE", 4) == 0) {  change_INT0(MODE_FREE);  return; }
    if (strncmp(ibuf+2, "HIGH", 4) == 0) {  change_INT0(MODE_HIGH);  return; }
    if (strncmp(ibuf+2, "LOW", 3) == 0)  {  change_INT0(MODE_LOW);   return; }
    if (strncmp(ibuf+2, "RST", 3) == 0)  {  setup();  return; }
    if (strncmp(ibuf+2, "TOHIGH",6)== 0) {  change_INT0(MODE_TOHIGH);  return; }
    if (strncmp(ibuf+2, "TOLOW", 5)== 0) {  change_INT0(MODE_TOLOW);   return; }
    if (strncmp(ibuf+2, "PAUSE", 5)== 0) {  to_work = 0;  Serial.print("!!Pause!!");   Serial.println(sCRLFs);  return; } // 暫停運作
    if (strncmp(ibuf+2, "RESUME",6)== 0) {  to_work = 1;  Serial.print("!!Resume!!");  Serial.println(sCRLFs);  return; } // 恢復運作
  }
}
// --------------------------------
void do_send()
{ // 把資料傳回電腦
  int   i, n;
  char  sbuf[16];
  if (! to_work) return;    n_sent ++;    // if (n_sent > 50) to_work = 0;
  if (busy_cnt) {  Serial.print("!! loss ");  Serial.print(busy_cnt);  Serial.println(" records");  }
  // Serial.println(sCRLFs);     // 減少電腦側訊息接收失誤 (PC 端不知為何每次必須讀滿 4 字元)
  // Serial.print("*L200");
  for (i=0;i < 32;i ++) xdata[i] = 65 + i;
  for (i=32;i < 64;i ++) xdata[i] = 0x20;
  n = cvt_8_to_7bit();        // 把 8-bit 轉成 7-bit 格式
  sprintf(sbuf, "*L%x", n);     Serial.print(sbuf);
  for (i=0;i < n;i ++) Serial.write(xdata7[i]);
  Serial.println("*OK*");
  // Serial.println(sCRLFs);     // 減少電腦側訊息接收失誤 (PC 端不知為何每次必須讀滿 4 字元)
  to_send = 0;    busy_cnt = 0;
  if (TrigMode == MODE_FREE) catch_data();  // 自動再取樣
}
// --------------------------------
void INT_ISR(uint32_t id, uint32_t event)   // INT 中斷處理 ! (for Ameba !!)
{ // INT0 中斷處理 !
  if (! to_work) return;
  if (to_send) {  busy_cnt ++;  return;  }  // 損失一次資料紀錄
  catch_data();  // 自動再取樣
  // Serial.print("INT id = ");  Serial.print(id);
  // Serial.print("  , event = ");  Serial.print(event);
}
// --------------------------------
void Say_Status()
{
  int  i;
  Serial.print("!!!!!! Trigger mode: ");
  Serial.println(sTrig[(int) TrigMode]);
  Serial.print("!!!!!! Delay counts = ");
  Serial.print(dly_cnt);    Serial.println(" counts");
  Serial.print("!!!!!! Sampling Delay counts = ");
  Serial.print(div_cnt);    Serial.println(" *10 counts");
  Serial.print("!!!!!! Sampling bit counts = ");
  i = bit_cnt;    Serial.println(i);
  Serial.println(sCRLFs);   // 減少電腦側訊息接收失誤 (PC 端不知為何每次必須讀滿 4 字元)
}
