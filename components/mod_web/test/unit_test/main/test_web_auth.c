#include "unity.h"
#include "mod_web_auth.h"
#include <string.h>

void setUp(void) { mod_web_auth_init(); }
void tearDown(void) {}

void test_set_and_verify_password(void)
{
    const char *pw = "correcthorsebatterystaple";
    // Set password
    TEST_ASSERT_EQUAL_INT(ESP_OK, mod_web_auth_set_password(pw));
    // Should be enabled now
    TEST_ASSERT_TRUE(mod_web_auth_is_enabled());
    // Verify correct password
    TEST_ASSERT_TRUE(mod_web_auth_verify_password(pw));
    // Verify wrong password
    TEST_ASSERT_FALSE(mod_web_auth_verify_password("wrongpassword"));
}

void test_token_generation_and_check(void)
{
    // Generate token
    char *token = mod_web_auth_generate_token(3600);
    TEST_ASSERT_NOT_NULL(token);

    // Build header string
    char auth_header[80];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", token);

    // Should be valid via helper
    TEST_ASSERT_TRUE(mod_web_auth_check_token_str(auth_header));

    // Modify token -> should fail
    auth_header[10] = (auth_header[10] == 'a') ? 'b' : 'a';
    TEST_ASSERT_FALSE(mod_web_auth_check_token_str(auth_header));

    free(token);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_set_and_verify_password);
    RUN_TEST(test_token_generation_and_check);
    return UNITY_END();
}
