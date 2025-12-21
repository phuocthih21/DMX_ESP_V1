/**
 * @file startup.h
 * @brief MOD_STARTUP Public API - Boot Manager
 * 
 * Provides boot mode decision, crash protection, and system stability monitoring.
 * This module runs FIRST in app_main() before any other module initialization.
 */

#pragma once

#include "startup_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decide boot mode based on crash history and button input
 * 
 * This is the main entry point for MOD_STARTUP. Call this function at the
 * very beginning of app_main() to determine how the system should initialize.
 * 
 * Decision algorithm:
 * 1. Analyze reset reason (power-on, software, crash/watchdog)
 * 2. Increment crash counter in NVS
 * 3. If crash counter >= 3, set mode to RESCUE
 * 4. Check physical boot button (highest priority override)
 * 5. Return final boot mode
 * 
 * Side effects:
 * - Increments crash counter in NVS
 * - Initializes NVS if not already initialized
 * - Configures GPIO for button detection
 * - May set status LED during button detection
 * 
 * @return Boot mode to use for this boot cycle
 */
boot_mode_t startup_decide_mode(void);

/**
 * @brief Get current boot mode
 * 
 * Returns the boot mode that was decided by startup_decide_mode().
 * Only valid after startup_decide_mode() has been called.
 * 
 * @return Current boot mode
 */
boot_mode_t startup_get_mode(void);

/**
 * @brief Mark system as stable (reset crash counter)
 * 
 * Call this function after the system has been running stably for a period
 * of time (typically 60 seconds). This resets the crash counter to 0 in NVS,
 * preventing false boot loop detection.
 * 
 * Automatically called by the 60-second stability timer in normal mode.
 * Can also be called manually if needed.
 * 
 * Thread-safety: YES (uses NVS mutex internally)
 */
void startup_mark_as_stable(void);

/**
 * @brief Start the stability timer (60 seconds)
 * 
 * Creates and starts a one-shot timer that will call startup_mark_as_stable()
 * after BOOT_STABLE_TIME_MS (60 seconds) if the system runs without crashing.
 * 
 * This should be called once during normal mode initialization after all
 * critical modules have been initialized.
 */
void boot_protect_start_stable_timer(void);

#ifdef __cplusplus
}
#endif
