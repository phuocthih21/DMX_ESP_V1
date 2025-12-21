#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

/** Initialize auth subsystem (NVS read). */
esp_err_t mod_web_auth_init(void);

/** Set admin password (stores hashed value in NVS) */
esp_err_t mod_web_auth_set_password(const char *password_plain);

/** Verify plain password against stored hash */
bool mod_web_auth_verify_password(const char *password_plain);

/** Check Authorization: Bearer <token> header */
bool mod_web_auth_check_request(httpd_req_t *req);

/** Check Authorization header string "Bearer <token>" (testable helper) */
bool mod_web_auth_check_token_str(const char *auth_header);

/** Generate a new session token and return it (owned by caller, must free) */
char *mod_web_auth_generate_token(size_t expiry_seconds);

/** Validate whether auth is enabled (password set) */
bool mod_web_auth_is_enabled(void);
