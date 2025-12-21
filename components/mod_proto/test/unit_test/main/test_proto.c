#include "unity.h"
#include "mod_proto.h"
#include "proto_types.h"

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
    return UNITY_END();
}
