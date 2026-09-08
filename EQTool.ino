#include <math.h>

#include "Display_ST7789.h"
#include "LVGL_Driver.h"
#include "XGZP6899A.h"
#include "Pressure_Sampler.h"
#include "ui.h"

/* ===================== 차트 표시 설정 =========================================
 *  차트 내부 값은 항상 Pa 로 넣는다 (분해능 확보).
 *  Y축 라벨만 그리기 이벤트에서 kPa 로 바꿔 그린다.
 *
 *  LVGL 8 의 lv_coord_t 는 int16_t 라 Pa 단위로는 +-32767 Pa 가 한계다.
 *  (= +-32.7 kPa. 풀스케일 +-40 kPa 전체를 보려면 Pa 대신 hPa 로 넣어야 한다)
 *
 *  0 Pa 위치: 아래에서 20% 지점 -> 위쪽 80%, 아래쪽 20%
 *  major tick 6개라 눈금이 -5, 0, 5, 10, 15, 20 kPa 로 딱 떨어진다.
 * ========================================================================== */
#define CHART_Y_MIN       -5000         /*  -5 kPa  (아래 20%) */
#define CHART_Y_MAX       20000         /* +20 kPa  (위  80%) */
#define CHART_Y_TICKS         6         /* major tick 수. 눈금 -5/0/5/10/15/20 */
#define CHART_POINTS        100
#define CHART_WINDOW_MS    2000         /* 차트 가로축이 담는 시간 폭 */
#define DRAW_MS              40         /* 화면 갱신 25 fps */

/* ===================== 로그 설정 ==========================================
 *   LOG_PERIODIC : 1초마다 압력값을 계속 출력한다. 평소에는 0 을 권장.
 *                  실행 중 시리얼 모니터에서 'p' 를 보내면 토글된다.
 *   그 외(부팅 정보, 센서 초기화 결과, 경고)는 항상 출력된다.
 *
 *   시리얼 명령:  p = 주기 로그 토글 / z = 영점 재설정 / i = 현재 상태 1회
 * ========================================================================= */
#define LOG_PERIODIC          0

static lv_chart_series_t *press_ser   = NULL;
static lv_chart_series_t *zero_ser    = NULL;   /* 0 기준선 */
static bool               log_periodic = (LOG_PERIODIC != 0);

static void draw_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    static double   acc   = 0.0;
    static uint32_t n     = 0;
    static uint32_t decim = 1;
    static uint32_t t_log = 0;

    uint32_t rate = Pressure_GetRateHz();
    if (rate > 0) {
        uint32_t d = (rate * CHART_WINDOW_MS / 1000) / CHART_POINTS;
        decim = (d < 1) ? 1 : d;
    }

    float pa;
    while (Pressure_Pop(&pa)) {
        acc += pa;
        if (++n >= decim) {
            long v = lroundf((float)(acc / n));          /* Pa 그대로 */
            if (v < CHART_Y_MIN) v = CHART_Y_MIN;
            if (v > CHART_Y_MAX) v = CHART_Y_MAX;
            lv_chart_set_next_value(ui_Chart1, press_ser, (lv_coord_t)v);
            acc = 0.0;
            n   = 0;
        }
    }

    uint32_t now = millis();
    if (now - t_log >= 1000) {
        t_log = now;

        /* --- 경고: 센서 미준비 (5초에 한 번만) --- */
        if (!Pressure_IsOk()) {
            static uint32_t t_warn = 0;
            if (now - t_warn >= 5000) {
                t_warn = now;
                Serial.printf("[EQTool] waiting for sensor - OUT=GPIO%d reads %.1f mV\n",
                              XGZP_ADC_PIN, XGZPA_ReadMv());
            }
            return;
        }

        /* --- 경고: 링버퍼 오버런 (발생했을 때만) --- */
        static uint32_t last_drop = 0;
        uint32_t drop = Pressure_GetDropped();
        if (drop != last_drop) {
            Serial.printf("[EQTool] WARNING: %u samples dropped (UI too slow)\n",
                          (unsigned)(drop - last_drop));
            last_drop = drop;
        }

        /* --- 주기 로그 (기본 off) --- */
        if (log_periodic)
            Serial.printf("P=%8.1f Pa   out=%7.1f mV   rate=%u Hz\n",
                          Pressure_GetFilteredPa(), Pressure_GetLastMv(),
                          (unsigned)Pressure_GetRateHz());
    }
}

/* 시리얼 한 글자 명령 */
static void Serial_Command(void)
{
    while (Serial.available()) {
        switch (Serial.read()) {
        case 'p':
        case 'P':
            log_periodic = !log_periodic;
            Serial.printf("[EQTool] periodic log %s\n", log_periodic ? "ON" : "OFF");
            break;
        case 'z':
        case 'Z':
            Serial.println("[EQTool] re-zeroing - keep both ports open");
            Pressure_Rezero();
            break;
        case 'i':
        case 'I':
            Serial.printf("[EQTool] P=%.1f Pa  out=%.1f mV  zero=%.1f mV  rate=%u Hz  drop=%u  heap=%u\n",
                          Pressure_GetFilteredPa(), Pressure_GetLastMv(),
                          XGZPA_GetZeroMv(), (unsigned)Pressure_GetRateHz(),
                          (unsigned)Pressure_GetDropped(), (unsigned)ESP.getFreeHeap());
            break;
        default:
            break;
        }
    }
}

/* Y축 눈금 라벨을 kPa 로 다시 쓴다.
   차트 값은 Pa 이므로 1000 으로 나눠 소수 첫째 자리까지 표기한다. */
static void chart_draw_event_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);

    if (!lv_obj_draw_part_check_type(dsc, &lv_chart_class, LV_CHART_DRAW_PART_TICK_LABEL))
        return;
    if (dsc->id != LV_CHART_AXIS_PRIMARY_Y || dsc->text == NULL)
        return;

    int32_t pa    = dsc->value;
    bool    minus = (pa < 0);
    int32_t a     = minus ? -pa : pa;

    lv_snprintf(dsc->text, dsc->text_length, "%s%d.%d",
                minus ? "-" : "", (int)(a / 1000), (int)((a % 1000) / 100));
}

static void Chart_Init(void)
{
    /* SquareLine 이 붙여둔 고정 배열(ext array) 시리즈를 제거하고 새로 만든다.
       그 배열은 값이 80개뿐인데 point_count 는 100 이라 그대로 두면 버퍼 오버런. */
    lv_chart_series_t *old = lv_chart_get_series_next(ui_Chart1, NULL);
    if (old) lv_chart_remove_series(ui_Chart1, old);

    lv_chart_set_type(ui_Chart1, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ui_Chart1, CHART_POINTS);
    lv_chart_set_range(ui_Chart1, LV_CHART_AXIS_PRIMARY_Y, CHART_Y_MIN, CHART_Y_MAX);

    /* 왼쪽 Y축: major tick 6개. draw_size 는 "-2.0" 이 들어갈 만큼만 */
    lv_chart_set_axis_tick(ui_Chart1, LV_CHART_AXIS_PRIMARY_Y,
                           10, 5, CHART_Y_TICKS, 2, true, 40);

    /* 라벨을 kPa 로 바꿔 그린다 */
    lv_obj_add_event_cb(ui_Chart1, chart_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

    /* 내부 눈금선.
       가로줄은 위/아래 끝을 포함해 균등 분할되므로, 축 tick 과 같은 6줄을 주면
       -5 / 0 / 5 / 10 / 15 / 20 kPa 즉 5 kPa 간격으로 정확히 떨어진다.
       세로줄 5개 = 2초 창을 0.5초 간격으로 나눈다. */
    lv_chart_set_div_line_count(ui_Chart1, CHART_Y_TICKS, 5);

    /* 눈금선 스타일 (LV_PART_MAIN. 데이터 선은 LV_PART_ITEMS 라 영향 없음) */
    lv_obj_set_style_line_color(ui_Chart1, lv_color_hex(0x2A4A7A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(ui_Chart1, 1,                      LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa  (ui_Chart1, LV_OPA_60,              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_chart_set_update_mode(ui_Chart1, LV_CHART_UPDATE_MODE_SHIFT);

    /* 오른쪽 보조 Y축 제거.
       SquareLine 이 SECONDARY_Y 에 draw_size 25 를 줘서 눈금과 여백이 남아 있다.
       인자: (obj, axis, major_len, minor_len, major_cnt, minor_cnt, label_en, draw_size)
       전부 0 으로 두면 눈금도 여백도 사라진다. */
    lv_chart_set_axis_tick(ui_Chart1, LV_CHART_AXIS_SECONDARY_Y, 0, 0, 0, 0, false, 0);

    /* 참고 - 다른 축도 지우고 싶으면 주석을 푸세요
    lv_chart_set_axis_tick(ui_Chart1, LV_CHART_AXIS_PRIMARY_X, 0, 0, 0, 0, false, 0);
    lv_chart_set_axis_tick(ui_Chart1, LV_CHART_AXIS_PRIMARY_Y, 0, 0, 0, 0, false, 0);
    */

    /* 0 기준선(노랑)을 먼저 추가한다.
       LVGL 은 먼저 추가된 시리즈를 아래에 그리므로, 데이터 선이 위에 온다. */
    zero_ser = lv_chart_add_series(ui_Chart1, lv_color_hex(0xFFE98A),
                                   LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(ui_Chart1, zero_ser, 0);

    press_ser = lv_chart_add_series(ui_Chart1, lv_color_hex(0x00E05A),
                                    LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(ui_Chart1, press_ser, 0);

    lv_timer_create(draw_timer_cb, DRAW_MS, NULL);
}

void setup()
{
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0 < 3000)) delay(10);
    delay(300);

    Serial.println();
    Serial.printf("[EQTool] boot  reset=%d  heap=%u   (serial: p=log  z=zero  i=info)\n",
                  (int)esp_reset_reason(), (unsigned)ESP.getFreeHeap());

    LCD_Init();
    Lvgl_Init();
    ui_init();

    Pressure_Start();
    Chart_Init();
}

void loop()
{
    Serial_Command();
    Timer_Loop();
    delay(1);
}
