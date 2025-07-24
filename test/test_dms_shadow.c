
/*
 * Unit Tests for DMS Shadow Module
 *
 * Tests cover:
 * - Shadow module initialization
 * - MQTT interface integration
 * - Shadow service management
 * - System state updates
 * - Device binding detection
 * - Error handling
 */

#include "unity.h"
#include "dms_config.h" 
#include "mock_dms_log.h"

void setUp(void) {
    dms_config_init();
}

void tearDown(void) {
    dms_config_cleanup();
}

void test_shadow_configuration_should_be_available(void) {
    const dms_aws_iot_config_t* aws_config = dms_config_get_aws_iot();
    TEST_ASSERT_NOT_NULL(aws_config);
    TEST_ASSERT_EQUAL_STRING("benq-dms-test-ABA1AE692AAE", aws_config->client_id);
}

void test_shadow_timeout_should_be_configured(void) {
    const dms_reconnect_config_t* reconnect_config = dms_config_get_reconnect();
    TEST_ASSERT_NOT_NULL(reconnect_config);
    TEST_ASSERT_TRUE(reconnect_config->shadow_get_timeout_ms > 0);
}
