#pragma once

#include "esp_err.h"

/** Initialize CPU sampling (idle-hook based) */
void sys_cpu_init(void);

/** vApplicationIdleHook is implemented in sys_cpu.c and will be called if CONFIG_FREERTOS_USE_IDLE_HOOK=y */
void vApplicationIdleHook(void);
