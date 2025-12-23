#include "sys_mod.h"
#include "dmx_types.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "portmacro.h"
#include <stdint.h>

static const char* TAG = "SYS_CPU";

/* Idle counters per core (incremented from idle hook) */
static volatile uint32_t s_idle_counters[2] = {0, 0};

/* Simple moving maximum used for normalization */
static float s_idle_max = 1.0f;

/* Previous aggregated idle counter value */
static uint32_t s_prev_idle_sum = 0;

/* Forward declaration */
void sys_cpu_task(void* arg);

/*
 * FreeRTOS Idle Hook - very small & fast
 * Called from each core idle task when CONFIG_FREERTOS_USE_IDLE_HOOK is enabled.
 */
void vApplicationIdleHook(void)
{
    BaseType_t core = xPortGetCoreID();
    if (core < 0 || core > 1) return;
    s_idle_counters[core]++;
}

void sys_cpu_task(void* arg)
{
    (void)arg;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1s window

        uint32_t sum = s_idle_counters[0] + s_idle_counters[1];
        uint32_t delta = sum - s_prev_idle_sum;
        s_prev_idle_sum = sum;

        if (delta == 0) {
            // No idle observed; assume high load
            s_idle_max = s_idle_max * 0.98f + 1.0f * 0.02f;
        } else {
            // Update moving maximum to track typical fully-idle value
            if ((float)delta > s_idle_max) {
                s_idle_max = (float)delta;
            } else {
                s_idle_max = s_idle_max * 0.98f + ((float)delta) * 0.02f;
            }
        }

        float idle_frac = (s_idle_max > 0.0f) ? ((float)delta / s_idle_max) : 0.0f;
        if (idle_frac > 1.0f) idle_frac = 1.0f;
        if (idle_frac < 0.0f) idle_frac = 0.0f;

        float cpu_percent = (1.0f - idle_frac) * 100.0f;
        if (cpu_percent < 0.0f) cpu_percent = 0.0f;
        if (cpu_percent > 100.0f) cpu_percent = 100.0f;

        sys_state_t* state = sys_get_state();
        if (state) {
            state->cpu_load = (uint8_t)(cpu_percent + 0.5f);
        }

        ESP_LOGD(TAG, "Idle delta=%u idle_max=%.1f cpu=%.1f", delta, s_idle_max, cpu_percent);
    }
}

void sys_cpu_init(void)
{
    // initialize cpu_load to zero
    sys_state_t* state = sys_get_state();
    if (state) state->cpu_load = 0;

    BaseType_t res = xTaskCreatePinnedToCore(sys_cpu_task, "sys_cpu", 2048, NULL, tskIDLE_PRIORITY + 1, NULL, tskNO_AFFINITY);
    if (res != pdPASS) {
        ESP_LOGW(TAG, "Failed to create cpu task");
    } else {
        ESP_LOGI(TAG, "CPU sampling task started");
    }
}