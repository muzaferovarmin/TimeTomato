#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif


U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 16, /* data=*/ 17);   // ESP32 Thing, HW I2C with pin remapping

void drawWord(String word){

  u8g2.clearBuffer();					// clear the internal memory
  u8g2.setFont(u8g2_font_helvB12_tf);	// choose a suitable font
  u8g2.drawStr(10,10,word);	// write something to the internal memory
  u8g2.sendBuffer();					// transfer internal memory to the display
  delay(1000); 
}