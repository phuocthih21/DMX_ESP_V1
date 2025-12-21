/**
 * @file mod_web_validation.h
 * @brief Input Validation Functions
 * 
 * Validates user input before applying to system.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validate DMX port index
 * 
 * @param port Port index (0-3)
 * @return true if valid
 */
bool mod_web_validation_port(int port);

/**
 * @brief Validate DMX universe ID
 * 
 * @param universe Universe ID (0-32767)
 * @return true if valid
 */
bool mod_web_validation_universe(int universe);

/**
 * @brief Validate DMX break time
 * 
 * @param break_us Break time in microseconds (88-500)
 * @return true if valid
 */
bool mod_web_validation_break_us(int break_us);

/**
 * @brief Validate DMX MAB time
 * 
 * @param mab_us Mark After Break time in microseconds (8-100)
 * @return true if valid
 */
bool mod_web_validation_mab_us(int mab_us);

/**
 * @brief Validate IP address format (basic check)
 * 
 * @param ip IP address string
 * @return true if format looks valid
 */
bool mod_web_validation_ip(const char *ip);

#ifdef __cplusplus
}
#endif

