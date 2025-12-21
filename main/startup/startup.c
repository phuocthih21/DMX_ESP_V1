/**
 * @file startup.c
 * @brief MOD_STARTUP main orchestration
 * 
 * Implements boot mode decision algorithm combining crash detection
 * and button input.
 */

#include "startup.h"
#include "startup_types.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

static const char* TAG = "STARTUP";

/* ========== FORWARD DECLARATIONS FROM OTHER FILES ========== */

// From crash_monitor.c
extern uint8_t boot_protect_increment(void);
extern void boot_protect_reset(void);
extern void boot_protect_start_stable_timer(void);

// From button_check.c
extern boot_mode_t check_button_on_boot(void);

/* ========== STARTUP STATE ========== */

typedef struct {
    boot_mode_t mode;              /**< Decided boot mode */
    esp_reset_reason_t reset_reason;
    bool was_crash;                /**< True if reset was due to crash */
} startup_state_t;

static startup_state_t g_startup_state = {0};

/* ========== PUBLIC API IMPLEMENTATION ========== */

boot_mode_t startup_decide_mode(void)
{
    boot_mode_t mode = BOOT_MODE_NORMAL;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  MOD_STARTUP - Boot Mode Decision");
    ESP_LOGI(TAG, "========================================");

    /* Step 1: Analyze reset reason */
    g_startup_state.reset_reason = esp_reset_reason();

    switch (g_startup_state.reset_reason) {
        case ESP_RST_POWERON:
            ESP_LOGI(TAG, "Reset reason: Power On");
            g_startup_state.was_crash = false;
            break;

        case ESP_RST_SW:
            ESP_LOGI(TAG, "Reset reason: Software Reset");
            g_startup_state.was_crash = false;
            break;

        case ESP_RST_PANIC:
            ESP_LOGE(TAG, "Reset reason: Exception/Panic");
            g_startup_state.was_crash = true;
            break;

        case ESP_RST_INT_WDT:
            ESP_LOGE(TAG, "Reset reason: Interrupt Watchdog");
            g_startup_state.was_crash = true;
            break;

        case ESP_RST_TASK_WDT:
            ESP_LOGE(TAG, "Reset reason: Task Watchdog");
            g_startup_state.was_crash = true;
            break;

        case ESP_RST_WDT:
            ESP_LOGE(TAG, "Reset reason: Other Watchdog");
            g_startup_state.was_crash = true;
            break;

        case ESP_RST_BROWNOUT:
            ESP_LOGW(TAG, "Reset reason: Brownout");
            g_startup_state.was_crash = false;  // Power issue, not code crash
            break;

        default:
            ESP_LOGW(TAG, "Reset reason: Unknown (%d)", g_startup_state.reset_reason);
            g_startup_state.was_crash = false;
            break;
    }

    /* Step 2: Increment crash counter (always) */
    uint8_t crash_count = boot_protect_increment();

    /* Step 3: Check crash counter threshold */
    if (crash_count >= BOOT_CRASH_THRESHOLD) {
        ESP_LOGE(TAG, "Crash counter exceeded (%d >= %d)", 
                 crash_count, BOOT_CRASH_THRESHOLD);
        ESP_LOGE(TAG, "Boot loop detected - forcing RESCUE mode");
        mode = BOOT_MODE_RESCUE;
        // Note: Don't return yet, button can still override to factory reset
    }

    /* Step 4: Check physical button (highest priority) */
    boot_mode_t button_mode = check_button_on_boot();
    if (button_mode != BOOT_MODE_NORMAL) {
        ESP_LOGW(TAG, "Button override detected");
        mode = button_mode;  // Button overrides crash detection
    }

    /* Step 5: Log final decision */
    g_startup_state.mode = mode;

    const char* mode_str;
    switch (mode) {
        case BOOT_MODE_NORMAL:
            mode_str = "NORMAL";
            break;
        case BOOT_MODE_RESCUE:
            mode_str = "RESCUE";
            break;
        case BOOT_MODE_FACTORY_RESET:
            mode_str = "FACTORY_RESET";
            break;
        default:
            mode_str = "UNKNOWN";
            break;
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Boot mode decided: %s", mode_str);
    ESP_LOGI(TAG, "========================================");

    return mode;
}

boot_mode_t startup_get_mode(void)
{
    return g_startup_state.mode;
}

void startup_mark_as_stable(void)
{
    boot_protect_reset();
}

/**
 * @brief Start stability timer (internal function, called by main.c)
 * 
 * This is not in the public API but is used by main.c after normal
 * mode initialization completes.
 */
void startup_begin_stability_timer(void)
{
    boot_protect_start_stable_timer();
}
