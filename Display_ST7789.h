#pragma once
#include <Arduino.h>
#include <SPI.h>
// ===== Screen rotation: 0 / 90 / 180 / 270 =====
#define LCD_ROTATION  90

#if (LCD_ROTATION == 0) || (LCD_ROTATION == 180)
  #define LCD_WIDTH   172   //LCD width  (portrait)
  #define LCD_HEIGHT  320   //LCD height (portrait)
#else
  #define LCD_WIDTH   320   //LCD width  (landscape)
  #define LCD_HEIGHT  172   //LCD height (landscape)
#endif

#define SPIFreq                        80000000
/* MISO 는 사용하지 않는다 (ST7789 는 쓰기 전용).
   GPIO5 를 I2C(SCL) 로 넘겨주기 위해 -1 로 해제한다. */
#define EXAMPLE_PIN_NUM_MISO           -1
#define EXAMPLE_PIN_NUM_MOSI           6
#define EXAMPLE_PIN_NUM_SCLK           7
#define EXAMPLE_PIN_NUM_LCD_CS         14
#define EXAMPLE_PIN_NUM_LCD_DC         15
#define EXAMPLE_PIN_NUM_LCD_RST        21
#define EXAMPLE_PIN_NUM_BK_LIGHT       22
#define Frequency       1000     
#define Resolution      10       

#define VERTICAL   0
#define HORIZONTAL 1

#if (LCD_ROTATION == 0) || (LCD_ROTATION == 180)
  #define Offset_X 34       // (240-172)/2 on the column axis
  #define Offset_Y 0
#else
  #define Offset_X 0
  #define Offset_Y 34       // (240-172)/2 moves to the row axis
#endif


void LCD_SetCursor(uint16_t x1, uint16_t y1, uint16_t x2,uint16_t y2);

void LCD_Init(void);
void LCD_SetCursor(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t  Yend);
void LCD_addWindow(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend,uint16_t* color);

void Backlight_Init(void);
void Set_Backlight(uint8_t Light);
