#include "mod_web_auth.h"
#include "sdkconfig.h"
#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL 0
#endif
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "mbedtls/sha256.h"
#include "esp_system.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Provide local prototypes to satisfy static analysis and avoid implicit declaration errors
   (these are provided by mbedtls and esp_system in the build environment). */
#ifdef __cplusplus
extern "C" {
#endif
int mbedtls_sha256_ret(const unsigned char *input, size_t ilen, unsigned char output[32], int is224);
uint32_t esp_random(void);
#ifdef __cplusplus
}
#endif

static const char *TAG = "MOD_WEB_AUTH";
static const char *NVS_NAMESPACE = "auth";
static const char *NVS_KEY_ADMIN_HASH = "admin_hash";

static char s_token[65]; // hex token (32 bytes -> 64 chars) + null
static int64_t s_token_expiry = 0; // esp_timer_get_time() microseconds

static void hash_password_hex(const char *password, char out_hex[65])
{
    unsigned char digest[32];
    /* Use mbedtls_sha256 (older and newer mbedtls variants may provide either name) */
    mbedtls_sha256((const unsigned char *)password, strlen(password), digest, 0);
    for (int i = 0; i < 32; i++) {
        sprintf(out_hex + (i * 2), "%02x", digest[i]);
    }
    out_hex[64] = '\0';
}

esp_err_t mod_web_auth_init(void)
{
    // NVS is expected to be initialized in system setup; ensure namespace accessible
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (ret == ESP_OK) {
        nvs_close(h);
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Auth NVS namespace not found (no password set)");
    } else {
        ESP_LOGW(TAG, "mod_web_auth_init: nvs_open returned %d", ret);
    }
    // No token yet
    s_token[0] = '\0';
    s_token_expiry = 0;
    return ESP_OK;
}

esp_err_t mod_web_auth_set_password(const char *password_plain)
{
    if (!password_plain) return ESP_ERR_INVALID_ARG;
    char hex[65];
    hash_password_hex(password_plain, hex);

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_str(h, NVS_KEY_ADMIN_HASH, hex);
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Admin password set (hash stored in NVS)");
    }
    return ret;
}

bool mod_web_auth_verify_password(const char *password_plain)
{
    if (!password_plain) return false;
    char hex[65];
    hash_password_hex(password_plain, hex);

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No stored admin hash");
        return false;
    }

    size_t required = 0;
    ret = nvs_get_str(h, NVS_KEY_ADMIN_HASH, NULL, &required);
    if (ret != ESP_OK || required != 65) {
        nvs_close(h);
        return false;
    }
    char stored[65];
    ret = nvs_get_str(h, NVS_KEY_ADMIN_HASH, stored, &required);
    nvs_close(h);
    if (ret != ESP_OK) return false;

    return (strncmp(hex, stored, 65) == 0);
}

bool mod_web_auth_is_enabled(void)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (ret != ESP_OK) return false;
    size_t required = 0;
    ret = nvs_get_str(h, NVS_KEY_ADMIN_HASH, NULL, &required);
    nvs_close(h);
    return (ret == ESP_OK && required == 65);
}

char *mod_web_auth_generate_token(size_t expiry_seconds)
{
    unsigned char rb[32];
    for (int i = 0; i < 32; i++) {
        rb[i] = (unsigned char)(esp_random() & 0xFF);
    }
    for (int i = 0; i < 32; i++) {
        sprintf(&s_token[i * 2], "%02x", rb[i]);
    }
    s_token[64] = '\0';
    int64_t now_us = esp_timer_get_time();
    s_token_expiry = now_us + ((int64_t)expiry_seconds * 1000000LL);

    // Return a heap-allocated copy
    char *r = strdup(s_token);
    return r;
}

static bool mod_web_auth_check_token_str_internal(const char *auth_header)
{
    if (!auth_header) return false;

    // Expect Bearer <token>
    const char *prefix = "Bearer ";
    if (strncmp(auth_header, prefix, strlen(prefix)) != 0) return false;
    const char *token = auth_header + strlen(prefix);
    if (strlen(token) != 64) return false;

    // Check token and expiry
    if (s_token[0] == '\0') return false;
    if (strncmp(token, s_token, 64) != 0) return false;
    int64_t now_us = esp_timer_get_time();
    if (s_token_expiry > 0 && now_us > s_token_expiry) return false;

    return true;
}

bool mod_web_auth_check_request(httpd_req_t *req)
{
    if (!mod_web_auth_is_enabled()) return false;
    if (!req) return false;

    // Get Authorization header
    char auth_header[128];
    size_t hdr_len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (hdr_len == 0 || hdr_len >= sizeof(auth_header)) return false;
    httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header));

    return mod_web_auth_check_token_str_internal(auth_header);
}

bool mod_web_auth_check_token_str(const char *auth_header)
{
    if (!mod_web_auth_is_enabled()) return false;
    return mod_web_auth_check_token_str_internal(auth_header);
}
