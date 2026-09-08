/*****************************************************************************
 *  XGZP6899A  -  아날로그 전압 출력 압력 센서 (CFSensor)
 *
 *  칩 마킹 "6899 / 040A" =  6899 시리즈 / 40 kPa / A = Analog output
 *  I2C 가 아니다. 출력 핀의 전압을 ADC 로 읽는다.
 *
 *  ---- 배선 (SOP8) --------------------------------------------------------
 *    pin 2 (GND)  -- GND
 *    pin 5 (OUT)  -- GPIO4   (ADC1_CH4)          <- 여기가 유일한 신호선
 *    pin 6 (VDD)  -- 3V3
 *    pin 1,3,4,7,8 = N/C  ->  아무것도 연결하지 말 것
 *
 *    OUT-GND 사이에 100nF 를 달면 노이즈가 줄어든다 (선택).
 * ------------------------------------------------------------------------ */
#pragma once
#include <Arduino.h>

#define XGZP_ADC_PIN        4          /* OUT 을 물린 GPIO (C6 의 ADC1: GPIO0~6) */
#define XGZP_VDD_MV         3300.0f    /* 실제 공급 전압(mV). 정확할수록 좋다 */
#define XGZP_FS_KPA         40.0f      /* 풀스케일 (마킹 040 = 40 kPa) */
#define XGZP_SPAN_RATIO     0.80f      /* 출력 스팬 = 10%~90% VDD */
#define XGZP_BIDIRECTIONAL  1          /* 1 = -40~+40 (DPN), 0 = 0~40 (DG) */
#define XGZP_AUTO_DETECT    1          /* 1 = 무압 출력 전압으로 DPN/DG 자동 판별 */
#define XGZP_OVERSAMPLE     8          /* 한 샘플당 ADC 평균 횟수 */

bool  XGZPA_Init(void);
float XGZPA_ReadMv(void);              /* 출력 전압 (mV), 오버샘플 평균 */
float XGZPA_MvToPa(float mv);          /* 전압 -> Pa */
void  XGZPA_AutoZero(uint32_t ms);     /* 무압 상태 기준점 잡기 */
float XGZPA_GetZeroMv(void);
void  XGZPA_SetZeroMv(float mv);
float XGZPA_SensMvPerKpa(void);
bool  XGZPA_IsBidirectional(void);
