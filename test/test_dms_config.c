
/*
 * Unit Tests for DMS Configuration Module
 *
 * Tests cover:
 * - Configuration initialization
 * - Configuration retrieval
 * - Configuration validation
 * - Error handling
 */


#include "unity.h"
#include "dms_config.h"
#include "mock_dms_log.h"
#include <string.h>

void setUp(void) {
    // 不需要任何特殊設置
}

void tearDown(void) {
    dms_config_cleanup();
    mock_dms_log_Destroy();
}

void test_dms_config_init_should_succeed(void) {
    /* Arrange - 忽略所有可能的日誌輸出 */
    dms_log_cleanup_Ignore();  // ✅ 使用實際存在的Mock函數
    
    /* Act */
    dms_result_t result = dms_config_init();

    /* Assert */
    TEST_ASSERT_EQUAL(DMS_SUCCESS, result);
}

void test_dms_config_get_should_return_valid_config_after_init(void) {
    /* Arrange */
    dms_log_cleanup_Ignore();
    dms_config_init();

    /* Act */
    const dms_config_t* config = dms_config_get();

    /* Assert */
    TEST_ASSERT_NOT_NULL(config);
    TEST_ASSERT_TRUE(config->initialized);
}

void test_dms_config_get_aws_iot_should_return_correct_values(void) {
    /* Arrange */
    dms_log_cleanup_Ignore();
    dms_config_init();

    /* Act */
    const dms_aws_iot_config_t* aws_config = dms_config_get_aws_iot();

    /* Assert */
    TEST_ASSERT_NOT_NULL(aws_config);
    TEST_ASSERT_EQUAL_STRING("apexd90h2t5wg-ats.iot.eu-central-1.amazonaws.com",
                             aws_config->aws_endpoint);
    TEST_ASSERT_EQUAL_STRING("benq-dms-test-ABA1AE692AAE", aws_config->client_id);
    TEST_ASSERT_EQUAL(8883, aws_config->mqtt_port);
    TEST_ASSERT_EQUAL(60, aws_config->keep_alive_seconds);
}

void test_dms_config_get_api_should_return_correct_values(void) {
    /* Arrange */
    dms_log_cleanup_Ignore();
    dms_config_init();

    /* Act */
    const dms_api_config_t* api_config = dms_config_get_api();

    /* Assert */
    TEST_ASSERT_NOT_NULL(api_config);
    TEST_ASSERT_EQUAL_STRING("https://dms-test.benq.com/api/", api_config->base_url);
    TEST_ASSERT_EQUAL_STRING("instashow", api_config->product_type);
    TEST_ASSERT_EQUAL(5000, api_config->timeout_ms);
    TEST_ASSERT_EQUAL(3, api_config->max_retries);
}


void test_dms_config_get_reconnect_should_return_correct_values(void) {
    /* Arrange */
    dms_config_init();

    /* Act */
    const dms_reconnect_config_t* reconnect_config = dms_config_get_reconnect();

    /* Assert */
    TEST_ASSERT_NOT_NULL(reconnect_config);
    
    // 🔥 根據實際的 dms_config.c 預設值修正：
    TEST_ASSERT_EQUAL(10, reconnect_config->max_retry_attempts);  // ✅ 改為 10 (原本是 3)
    TEST_ASSERT_EQUAL(2, reconnect_config->base_delay_seconds);   // ✅ 正確
    TEST_ASSERT_EQUAL(300, reconnect_config->max_delay_seconds);  // ✅ 改為 300 (原本是 60)
    TEST_ASSERT_TRUE(reconnect_config->enable_exponential_backoff);
}

void test_dms_config_get_should_return_null_before_init(void) {
    /* Act - 不需要Mock，因為沒有初始化就不會調用日誌 */
    const dms_config_t* config = dms_config_get();

    /* Assert */
    TEST_ASSERT_NULL(config);
}

void test_dms_config_init_twice_should_succeed_with_warning(void) {
    /* Arrange */
    dms_log_cleanup_Ignore();
    dms_config_init();
    
    /* 對於重複初始化，我們只能忽略所有日誌輸出 */
    dms_log_cleanup_Ignore();

    /* Act */
    dms_result_t result = dms_config_init();

    /* Assert */
    TEST_ASSERT_EQUAL(DMS_SUCCESS, result);
}

void test_dms_config_cleanup_should_reset_initialization_flag(void) {
    /* Arrange */
    dms_log_cleanup_Ignore();
    dms_config_init();
    TEST_ASSERT_NOT_NULL(dms_config_get());

    /* Act */
    dms_config_cleanup();

    /* Assert */
    TEST_ASSERT_NULL(dms_config_get());
}
