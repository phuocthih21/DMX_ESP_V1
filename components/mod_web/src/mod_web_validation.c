/**
 * @file mod_web_validation.c
 * @brief Input Validation Implementation
 */

#include "mod_web_validation.h"
#include <string.h>
#include <ctype.h>

bool mod_web_validation_port(int port)
{
    return (port >= 0 && port < 4);
}

bool mod_web_validation_universe(int universe)
{
    return (universe >= 0 && universe <= 32767);
}

bool mod_web_validation_break_us(int break_us)
{
    return (break_us >= 88 && break_us <= 500);
}

bool mod_web_validation_mab_us(int mab_us)
{
    return (mab_us >= 8 && mab_us <= 100);
}

bool mod_web_validation_ip(const char *ip)
{
    if (ip == NULL || strlen(ip) == 0) {
        return false;
    }

    // Basic format check: should contain dots and numbers
    int dots = 0;
    int digits = 0;
    for (const char *p = ip; *p != '\0'; p++) {
        if (*p == '.') {
            dots++;
        } else if (isdigit((unsigned char)*p)) {
            digits++;
        } else {
            return false;
        }
    }

    // Should have 3 dots and at least 7 digits (minimum: 0.0.0.0)
    return (dots == 3 && digits >= 7 && digits <= 15);
}

