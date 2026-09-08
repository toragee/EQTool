#include "Pressure_Sampler.h"
#include "XGZP6899A.h"

/* =========================== 튜닝 노브 =====================================
 *  ADC 는 I2C 와 달리 대기 시간이 거의 없다. 1ms 주기 = 1 kHz 로 충분히 돈다.
 *  더 빠르게 하려면 SAMPLE_TICKS 를 유지한 채 XGZP_OVERSAMPLE 을 줄이거나,
 *  vTaskDelay 대신 delayMicroseconds 로 바꿔야 한다 (UI 가 밀리니 주의).
 * ========================================================================= */
#define SAMPLE_TICKS      1            /* FreeRTOS tick = 1ms -> 약 1 kHz */
#define AUTOZERO_MS       500
#define FILTER_ALPHA      0.03f        /* 표시용 IIR. 작을수록 매끄럽고 느리다 */

/* ---- 링버퍼 (단일 생산자 / 단일 소비자, 락 없음) ------------------------- */
#define RB_SIZE      2048              /* 2의 거듭제곱이어야 함 */
#define RB_MASK      (RB_SIZE - 1)

static float             rb[RB_SIZE];
static volatile uint32_t rb_head    = 0;
static volatile uint32_t rb_tail    = 0;
static volatile uint32_t rb_dropped = 0;

/* ---- 상태 --------------------------------------------------------------- */
static volatile float    last_pa  = 0.0f;
static volatile float    filt_pa  = 0.0f;
static volatile float    last_mv  = 0.0f;
static volatile uint32_t rate_hz  = 0;
static volatile bool     ok       = false;
static volatile bool     rezero_req = false;

static void sampler_task(void *arg)
{
    (void)arg;

    /* 센서가 정상 범위를 낼 때까지 기다린다 (배선 중에 꽂아도 잡히도록) */
    while (!XGZPA_Init()) vTaskDelay(pdMS_TO_TICKS(1000));

    XGZPA_AutoZero(AUTOZERO_MS);
    ok = true;

    uint32_t n_sec = 0;
    uint32_t t_sec = millis();

    for (;;) {
        if (rezero_req) {
            rezero_req = false;
            XGZPA_AutoZero(AUTOZERO_MS);
        }

        float mv = XGZPA_ReadMv();
        float pa = XGZPA_MvToPa(mv);
        last_mv = mv;
        last_pa = pa;
        filt_pa = filt_pa + (pa - filt_pa) * FILTER_ALPHA;

        uint32_t h = rb_head;
        rb[h & RB_MASK] = pa;
        rb_head = h + 1;
        if (rb_head - rb_tail > RB_SIZE) rb_dropped++;
        n_sec++;

        uint32_t now = millis();
        if (now - t_sec >= 1000) {
            rate_hz = (uint32_t)((uint64_t)n_sec * 1000 / (now - t_sec));
            n_sec = 0;
            t_sec = now;
        }

        vTaskDelay(SAMPLE_TICKS);
    }
}

/* ---- 공개 API ------------------------------------------------------------ */
bool Pressure_Start(void)
{
    ok = false;
    BaseType_t r = xTaskCreate(sampler_task, "xgzp", 4096, NULL, 1, NULL);
    if (r != pdPASS) { Serial.println("[SAMP] task create failed"); return false; }
    return true;
}

bool Pressure_Pop(float *pa)
{
    uint32_t h = rb_head;
    uint32_t t = rb_tail;
    if (t == h) return false;

    if (h - t > RB_SIZE) t = h - RB_SIZE;      /* 오버런 시 오래된 것 버림 */
    *pa = rb[t & RB_MASK];
    rb_tail = t + 1;
    return true;
}

uint32_t Pressure_GetRateHz(void)  { return rate_hz; }
uint32_t Pressure_GetDropped(void) { return rb_dropped; }
float    Pressure_GetLastPa(void)  { return last_pa; }
float    Pressure_GetFilteredPa(void) { return filt_pa; }
float    Pressure_GetLastMv(void)  { return last_mv; }
bool     Pressure_IsOk(void)       { return ok; }
void     Pressure_Rezero(void)     { rezero_req = true; }
