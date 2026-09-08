# EQTool

ESP32-C6 기반 압력 측정 도구. Waveshare **ESP32-C6-LCD-1.47** 보드에서
**XGZP6899A** 아날로그 압력 센서를 읽어 LVGL 차트로 실시간 표시한다.

## 하드웨어

| 항목 | 내용 |
|---|---|
| MCU 보드 | Waveshare ESP32-C6-LCD-1.47 (ST7789 172x320) |
| 센서 | CFSensor XGZP6899A (40 kPa, 아날로그 전압 출력) |
| 화면 방향 | 가로 (320 x 172), `LCD_ROTATION 90` |

### 배선

| 센서 핀 | 신호 | ESP32-C6 |
|---|---|---|
| 2 | GND | GND |
| 5 | OUT | **GPIO4** (ADC1_CH4) |
| 6 | VDD | 3V3 |
| 1, 3, 4, 7, 8 | N/C | 연결하지 않음 |

> XGZP6899**A** 는 아날로그 출력이다. I2C 모델(XGZP6899**D**)과 핀 배치가 다르다.
> 칩 마킹 `6899 / 040A` 에서 `A` 가 Analog, `040` 이 40 kPa 를 뜻한다.

## 소프트웨어 구성

| 파일 | 역할 |
|---|---|
| `EQTool.ino` | 진입점, LVGL 차트 설정, 시리얼 명령 |
| `XGZP6899A.h/.cpp` | 센서 드라이버 (ADC 읽기, 전압→압력 변환, 오토제로) |
| `Pressure_Sampler.h/.cpp` | FreeRTOS 샘플링 태스크 + 락 없는 링버퍼 |
| `Display_ST7789.h/.cpp` | LCD 드라이버 (회전 지원) |
| `LVGL_Driver.h/.cpp` | LVGL 연결 |
| `ui*.c/.h` | SquareLine Studio 1.6.2 export (LVGL 8.3) |

센서 샘플링(약 800 Hz)은 별도 태스크에서 돌고, UI 는 25 fps 로 링버퍼를 비우며
샘플레이트에 맞춰 자동으로 압축(decimation)한다.

## 측정 원리

비율식(ratiometric) 아날로그 출력을 ADC 로 읽는다.

```
sens  = 0.80 x VDD / (2 x FS)          # 양방향(DPN) 기준
P(Pa) = (out_mV - zero_mV) / sens x 1000
```

부팅 시 0.5 초간 평균을 내어 영점을 잡고, 그 전압이 VDD 의 몇 % 인지로
DPN(양방향) / DG(단방향) 를 자동 판별한다.

## 빌드

- Arduino IDE 2.x
- 보드: **ESP32C6 Dev Module** (esp32 코어 3.x)
- 라이브러리: **lvgl 8.3.x** — `libraries/` 옆에 `lv_conf.h` 필요
- Tools 설정
  - USB CDC On Boot: **Enabled**
  - CPU Frequency: 160MHz (WiFi)
  - JTAG Adapter: Disabled
  - Flash Size: 4MB, Partition: Default 4MB with spiffs

## 시리얼 명령 (115200)

| 키 | 동작 |
|---|---|
| `p` | 주기 로그 on/off |
| `z` | 영점 재설정 (양쪽 포트 개방 상태에서) |
| `i` | 현재 상태 1회 출력 |

## 주요 설정값

`EQTool.ino`

```c
#define CHART_Y_MIN       -5000   // -5 kPa  (아래 20%)
#define CHART_Y_MAX       20000   // +20 kPa (위 80%)
#define CHART_WINDOW_MS    2000   // 가로축 2초
```

`XGZP6899A.h`

```c
#define XGZP_ADC_PIN          4
#define XGZP_VDD_MV      3300.0f
#define XGZP_FS_KPA        40.0f
#define XGZP_OVERSAMPLE       8
```

> LVGL 8 의 `lv_coord_t` 는 int16 이라 차트 값은 Pa 단위로 ±32767 이 한계다.
> Y축 라벨은 그리기 이벤트에서 kPa 로 변환해 표시한다.
