#include "unity.h"
#include "mod_proto.h"
#include "proto_types.h"

/* sacn helpers (internal) used by tests */
extern size_t sacn_get_joined_universes(uint16_t *out, size_t max);


/* Minimal unit tests for parsers and merge logic */

void setUp(void) {}
void tearDown(void) {}

void test_artnet_parse(void)
{
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "Art-Net", 7);
    /* opcode little-endian 0x5000 */
    buf[8] = 0x00; buf[9] = 0x50;
    /* prot ver */
    buf[10] = 0x00; buf[11] = 0x0e;
    /* sequence */ buf[12] = 0x01;
    buf[13] = 0x00; /* physical */
    buf[14] = 0x00; /* sub_uni */
    buf[15] = 0x00; /* net */
    /* length big-endian */
    buf[16] = 0x00; buf[17] = 0x04;
    buf[18] = 0x11; buf[19] = 0x22; buf[20] = 0x33; buf[21] = 0x44;

    uint16_t uni = 0;
    const uint8_t *data = NULL;
    uint16_t dlen = 0;
    int rc = parse_artnet_packet(buf, sizeof(buf), &uni, &data, &dlen);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_EQUAL_UINT16(0, uni);
    TEST_ASSERT_EQUAL_UINT16(4, dlen);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_HEX8(0x11, data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x44, data[3]);
}

void test_sacn_parse(void)
{
    /* construct minimal sACN packet per offsets used in parser */
    uint8_t buf[256];
    memset(buf, 0, sizeof(buf));
    /* preamble/postamble */
    buf[4] = 'A'; buf[5] = 'S'; buf[6] = 'C'; buf[7] = '-'; buf[8] = 'E'; buf[9] = '1'; buf[10] = '.'; buf[11] = '1';
    /* priority at offset 110 */ buf[110] = 100;
    /* universe at 113..114 */ buf[113] = 0x00; buf[114] = 0x01;
    /* prop_val_count at 123..124 */ buf[123] = 0x02; buf[124] = 0x01; /* 513? we'll use small */
    /* start code + data at 125 */ buf[125] = 0x00; /* start code */
    buf[126] = 0xAA; buf[127] = 0xBB; /* first two channels */

    uint16_t uni = 0; const uint8_t *data = NULL; uint16_t dlen = 0; uint8_t priority = 0;
    int rc = parse_sacn_packet(buf, sizeof(buf), &uni, &data, &dlen, &priority);
    TEST_ASSERT_EQUAL_INT(1, rc);
    TEST_ASSERT_EQUAL_UINT16(1, uni);
    TEST_ASSERT_EQUAL_UINT16(1, dlen); /* prop_val_count - 1 */
    TEST_ASSERT_EQUAL_UINT8(100, priority);
    TEST_ASSERT_EQUAL_HEX8(0xAA, data[0]);
}

void test_htp_merge(void)
{
    merge_init();
    /* prepare two sources for universe 0 which maps to port 0 by default */
    uint8_t a[DMX_UNIVERSE_SIZE]; uint8_t b[DMX_UNIVERSE_SIZE];
    memset(a, 0, sizeof(a)); memset(b, 0, sizeof(b));
    a[0] = 100; a[1] = 50;
    b[0] = 80; b[1] = 200;

    /* feed first source (src_ip 1) */
    merge_input_by_universe(0, a, DMX_UNIVERSE_SIZE, 100, 0x01000001);
    /* feed second source (src_ip 2) */
    merge_input_by_universe(0, b, DMX_UNIVERSE_SIZE, 100, 0x02000002);

    uint8_t *out = sys_get_dmx_buffer(0);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_HEX8(100, out[0]);
    TEST_ASSERT_EQUAL_HEX8(200, out[1]);
}

void test_ltp_merge(void)
{
    merge_init();
    /* set runtime to LTP */
    mod_proto_set_merge_mode(0, MERGE_MODE_LTP);

    uint8_t a[DMX_UNIVERSE_SIZE]; uint8_t b[DMX_UNIVERSE_SIZE];
    memset(a, 0, sizeof(a)); memset(b, 0, sizeof(b));
    a[0] = 100; a[1] = 50;
    b[0] = 80; b[1] = 200;

    /* feed first source (older) */
    merge_input_by_universe(0, a, DMX_UNIVERSE_SIZE, 100, 0x01000001);
    vTaskDelay(pdMS_TO_TICKS(10));
    /* feed second source (newer) */
    merge_input_by_universe(0, b, DMX_UNIVERSE_SIZE, 100, 0x02000002);

    uint8_t *out = sys_get_dmx_buffer(0);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_HEX8(80, out[0]);
    TEST_ASSERT_EQUAL_HEX8(200, out[1]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_artnet_parse);
    RUN_TEST(test_sacn_parse);
    RUN_TEST(test_htp_merge);
    RUN_TEST(test_ltp_merge);
    RUN_TEST(test_proto_reload_join_leave);
    RUN_TEST(test_priority_override);
    RUN_TEST(test_priority_override_ltp);
    return UNITY_END();
}

void test_proto_reload_join_leave(void)
{
    // Ensure proto stack is running and registered for events
    mod_proto_init();

    // Start with no joins
    uint16_t out[64];
    size_t n = sacn_get_joined_universes(out, 64);

    // Apply DMX config: port0 -> sACN universe 1, port1 -> sACN universe 2
    sys_dmx_port_status_t cfg[4];
    memset(cfg, 0, sizeof(cfg));
    cfg[0].enabled = true; cfg[0].protocol = PROTOCOL_SACN; cfg[0].universe = 1; cfg[0].fps = 40;
    cfg[1].enabled = true; cfg[1].protocol = PROTOCOL_SACN; cfg[1].universe = 2; cfg[1].fps = 40;

    TEST_ASSERT_EQUAL_INT(SYS_OK, sys_apply_dmx_config(cfg, 4));

    // Now joined universes should include 1 and 2
    n = sacn_get_joined_universes(out, 64);
    bool has1 = false, has2 = false;
    for (size_t i = 0; i < n; ++i) {
        if (out[i] == 1) has1 = true;
        if (out[i] == 2) has2 = true;
    }
    TEST_ASSERT_TRUE(has1);
    TEST_ASSERT_TRUE(has2);

    // Change config: only port0 -> sACN universe 3
    memset(cfg, 0, sizeof(cfg));
    cfg[0].enabled = true; cfg[0].protocol = PROTOCOL_SACN; cfg[0].universe = 3; cfg[0].fps = 40;

    TEST_ASSERT_EQUAL_INT(SYS_OK, sys_apply_dmx_config(cfg, 4));

    // Now joined universes should include 3 and not include 1
    n = sacn_get_joined_universes(out, 64);
    has1 = false; bool has3 = false;
    for (size_t i = 0; i < n; ++i) {
        if (out[i] == 1) has1 = true;
        if (out[i] == 3) has3 = true;
    }
    TEST_ASSERT_FALSE(has1);
    TEST_ASSERT_TRUE(has3);

    mod_proto_deinit();
}

void test_priority_override(void)
{
    merge_init();

    uint8_t a[DMX_UNIVERSE_SIZE]; uint8_t b[DMX_UNIVERSE_SIZE];
    memset(a, 0, sizeof(a)); memset(b, 0, sizeof(b));
    a[0] = 10; a[1] = 20;
    b[0] = 200; b[1] = 30;

    // a lower priority, b higher -> b should win entirely
    merge_input_by_universe(0, a, DMX_UNIVERSE_SIZE, 50, 0x01000001);
    merge_input_by_universe(0, b, DMX_UNIVERSE_SIZE, 100, 0x02000002);

    uint8_t *out = sys_get_dmx_buffer(0);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_HEX8(200, out[0]);
    TEST_ASSERT_EQUAL_HEX8(30, out[1]);
}

void test_priority_override_ltp(void)
{
    merge_init();
    // set runtime to LTP
    mod_proto_set_merge_mode(0, MERGE_MODE_LTP);

    uint8_t a[DMX_UNIVERSE_SIZE]; uint8_t b[DMX_UNIVERSE_SIZE];
    memset(a, 0, sizeof(a)); memset(b, 0, sizeof(b));
    a[0] = 10; a[1] = 20;
    b[0] = 200; b[1] = 30;

    // a lower priority, b higher -> b should win entirely despite LTP semantics
    merge_input_by_universe(0, a, DMX_UNIVERSE_SIZE, 50, 0x01000001);
    merge_input_by_universe(0, b, DMX_UNIVERSE_SIZE, 100, 0x02000002);

    uint8_t *out = sys_get_dmx_buffer(0);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_HEX8(200, out[0]);
    TEST_ASSERT_EQUAL_HEX8(30, out[1]);
}
}
