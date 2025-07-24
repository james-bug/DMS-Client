

/*
 * Unit Tests for DMS AWS IoT Module
 *
 * Tests cover:
 * - Module initialization
 * - Connection establishment
 * - MQTT operations
 * - Error handling
 * - State management
 */



#include "unity.h"
#include "dms_config.h"  // 只測試配置相關功能
#include "mock_dms_log.h"

// 暫時不直接測試 dms_aws_iot.h，先測試配置是否正確

void setUp(void) {
    // 基本設置
}

void tearDown(void) {
    dms_config_cleanup();
}

void test_aws_iot_config_should_be_accessible_via_dms_config(void) {
    /* Arrange */
    dms_config_init();

    /* Act */
    const dms_aws_iot_config_t* aws_config = dms_config_get_aws_iot();

    /* Assert */
    TEST_ASSERT_NOT_NULL(aws_config);
    TEST_ASSERT_EQUAL_STRING("apexd90h2t5wg-ats.iot.eu-central-1.amazonaws.com",
                             aws_config->aws_endpoint);
    TEST_ASSERT_EQUAL_STRING("benq-dms-test-ABA1AE692AAE", aws_config->client_id);
    TEST_ASSERT_EQUAL(8883, aws_config->mqtt_port);
}

void test_aws_iot_config_certificates_paths_should_be_correct(void) {
    /* Arrange */
    dms_config_init();

    /* Act */
    const dms_aws_iot_config_t* aws_config = dms_config_get_aws_iot();

    /* Assert */
    TEST_ASSERT_NOT_NULL(aws_config);
    TEST_ASSERT_EQUAL_STRING("/etc/dms-client/rootCA.pem", aws_config->ca_cert_path);
    TEST_ASSERT_EQUAL_STRING("/etc/dms-client/dms_pem.crt", aws_config->client_cert_path);
    TEST_ASSERT_EQUAL_STRING("/etc/dms-client/dms_private.pem.key", aws_config->private_key_path);
}

void test_aws_iot_config_timeouts_should_be_correct(void) {
    /* Arrange */
    dms_config_init();

    /* Act */
    const dms_aws_iot_config_t* aws_config = dms_config_get_aws_iot();

    /* Assert */
    TEST_ASSERT_NOT_NULL(aws_config);
    TEST_ASSERT_EQUAL(60, aws_config->keep_alive_seconds);
    TEST_ASSERT_EQUAL(1000, aws_config->connack_recv_timeout_ms);
    TEST_ASSERT_EQUAL(5000, aws_config->transport_timeout_ms);
}

// 添加更多配置相關的測試...
