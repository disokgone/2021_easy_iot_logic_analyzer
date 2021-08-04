#include <Wire.h>   // C:\Program Files (x86)\Arduino\hardware\arduino\avr\libraries\Wire\src
#include <Adafruit_SI5351.h>  // in E:\UNO_D1R2\scripts\libraries\Adafruit_Si5351_Library
#define LED_BUILTIN   13
#define SCL   22
#define SDA   21
Adafruit_SI5351 clockgen = Adafruit_SI5351();
// A4 => SDA, A5 => SCL (for I2C to Si5351)
void setup() {
  // 有 SI5351_PLL_A 跟 SI5351_PLL_B
  // SI5351_MULTISYNTH_DIV_4 (三種 .._DIV_4, .._DIV_6,  .._DIV_8)
  // SI5351_R_DIV_1 (可除 1/2/4/8/16/32/64/128)

  pinMode(LED_BUILTIN, OUTPUT);   // D13 as led indicator
  Serial.begin(9600);
  Serial.println("baud rate = 9600 bps");
  Serial.println("-- DOIT ESP32 DEVKIT V1");
  Serial.println("\n\nSi5351 Clock Generator Test !");
  if (clockgen.begin() != ERROR_NONE){
    /* There was a problem detecting the IC ... check your connections */
    Serial.print("Ooops, no Si5351 detected ... Check your wiring or I2C ADDR!");
  }
  Serial.println("Si5351 is OK !");
  /* INTEGER ONLY MODE --> most accurate output */  
  /* Setup PLLA to integer only mode @ 900MHz (must be 600..900MHz) */
  /* Set Multisynth 0 to 112.5MHz using integer only mode (div by 4/6/8) */
  /* 25MHz * 36 = 900 MHz, then 900 MHz / 8 = 112.5 MHz */
  Serial.println("Set PLLA to 900MHz");
  clockgen.setupPLLInt(SI5351_PLL_A, 16); // 乘數必須在[15,90]之間, 我們晶體是 25MHz (25 * 16 = 400MHz)
  Serial.println("Set Output #0 to 50 MHz");  
  clockgen.setupMultisynthInt(0, SI5351_PLL_A, SI5351_MULTISYNTH_DIV_8); // 僅三種: DIV_4/6/8

  /* FRACTIONAL MODE --> More flexible but introduce clock jitter */
  /* Setup PLLB to fractional mode @616.66667MHz (XTAL * 24 + 2/3) */
  /* Setup Multisynth 1 to 13.55311MHz (PLLB/45.5) */
  // clockgen.setupPLL(SI5351_PLL_B, 24, 2, 3);  // 25*24.666=616.66
  // Serial.println("Set Output #1 to 13.553115MHz");
  // clockgen.setupMultisynth(1, SI5351_PLL_B, 45, 1, 2);  // 616.6666666 / 45.5 = 13.55311
  clockgen.setupPLL(SI5351_PLL_B, 16, 0, 1);  // 乘數必須在[15,90]之間, 25*16=400 MHz,分子(20-bits)/分母(20-bits)
  Serial.println("Set Output #1 to 62.5 k8Hz");
  // setupMultisynth() 除數若非此三種: DIV_4/6/8,就只能除 [8,900],分子(20-bits)/分母(20-bits)
  clockgen.setupMultisynth(1, SI5351_PLL_B, 400, 0, 1);   // 400M / 400 = 1 MHz
  clockgen.setupRdiv(1, SI5351_R_DIV_64);   // 1 MHz / 64 = 15.625 kHz

  /* Multisynth 2 is not yet used and won't be enabled, but can be */
  /* Use PLLB @ 616.66667MHz, then divide by 900 -> 685.185 KHz */
  /* then divide by 64 for 10.706 KHz */
  /* configured using either PLL in either integer or fractional mode */

  //Serial.println("Set Output #2 to 10.706 KHz");  
  //clockgen.setupMultisynth(2, SI5351_PLL_B, 900, 0, 1); // 616.666666 / 900 = 0.685185185 MHz
  //clockgen.setupRdiv(2, SI5351_R_DIV_64);   // 0.685185185 MHz / 64 = 10.706 KHz

  Serial.println("Set Output #2 to 10 KHz");
  // 目前 PLL_B = 400 MHz
  // setupMultisynth() 除數若非此三種: DIV_4/6/8,就只能除 [8,900],分子(20-bits)/分母(20-bits)
  clockgen.setupMultisynth(2, SI5351_PLL_B, 900, 0, 1); // 400M / 900 = 444.44 KHz
  // Rdiv 可除: 2^1 .. 2^7 (七種)
  clockgen.setupRdiv(2, SI5351_R_DIV_128);   // 444.44 KHz / 128 = 3472.21 Hz
  
  /* Enable the clocks */
  clockgen.enableOutputs(true);
  // Serial.begin(500000);   // 500k BAUD RATE IS OK !!
  Serial.println("Ok !");
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  Serial.print(".");
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
}
