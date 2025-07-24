
/*
 * DMS Client Configuration Management Module Implementation
 * 修正版本：使用 demo_config.h 中正確的錯誤碼名稱
 */

#include "dms_config.h"
#include "dms_log.h"
#include <string.h>
#include <stdio.h>

/*-----------------------------------------------------------*/
/* 內部全域變數 */

static dms_config_t g_config = {0};
static bool g_config_initialized = false;

/*-----------------------------------------------------------*/
/* 內部函數宣告 */

static void load_default_aws_iot_config(dms_aws_iot_config_t* config);
static void load_default_api_config(dms_api_config_t* config);
static void load_default_reconnect_config(dms_reconnect_config_t* config);
static dms_result_t validate_aws_iot_config(const dms_aws_iot_config_t* config);
static dms_result_t validate_api_config(const dms_api_config_t* config);
static dms_result_t validate_reconnect_config(const dms_reconnect_config_t* config);

/*-----------------------------------------------------------*/
/* 公開介面實作 */

dms_result_t dms_config_init(void) {
    if (g_config_initialized) {
        DMS_LOG_WARN("Configuration already initialized");
        return DMS_SUCCESS;
    }

    DMS_LOG_INFO("Initializing DMS configuration...");

    // 清空配置結構
    memset(&g_config, 0, sizeof(g_config));

    // 載入預設配置 (從原本的 #define 值)
    load_default_aws_iot_config(&g_config.aws_iot);
    load_default_api_config(&g_config.api);
    load_default_reconnect_config(&g_config.reconnect);

    // 驗證配置
    dms_result_t result = dms_config_validate();
    if (result != DMS_SUCCESS) {
        DMS_LOG_ERROR("Configuration validation failed: %d", result);
        return result;
    }

    g_config.initialized = true;
    g_config_initialized = true;

    DMS_LOG_INFO("DMS configuration initialized successfully");
    DMS_LOG_DEBUG("AWS IoT Endpoint: %s", g_config.aws_iot.aws_endpoint);
    DMS_LOG_DEBUG("Client ID: %s", g_config.aws_iot.client_id);
    DMS_LOG_DEBUG("API Base URL: %s", g_config.api.base_url);

    return DMS_SUCCESS;
}

const dms_config_t* dms_config_get(void) {
    if (!g_config_initialized) {
        DMS_LOG_ERROR("Configuration not initialized");
        return NULL;
    }
    return &g_config;
}

const dms_aws_iot_config_t* dms_config_get_aws_iot(void) {
    if (!g_config_initialized) {
        DMS_LOG_ERROR("Configuration not initialized");
        return NULL;
    }
    return &g_config.aws_iot;
}

const dms_api_config_t* dms_config_get_api(void) {
    if (!g_config_initialized) {
        DMS_LOG_ERROR("Configuration not initialized");
        return NULL;
    }
    return &g_config.api;
}

const dms_reconnect_config_t* dms_config_get_reconnect(void) {
    if (!g_config_initialized) {
        DMS_LOG_ERROR("Configuration not initialized");
        return NULL;
    }
    return &g_config.reconnect;
}

dms_result_t dms_config_validate(void) {
    // 驗證 AWS IoT 配置
    dms_result_t result = validate_aws_iot_config(&g_config.aws_iot);
    if (result != DMS_SUCCESS) {
        return result;
    }

    // 驗證 API 配置
    result = validate_api_config(&g_config.api);
    if (result != DMS_SUCCESS) {
        return result;
    }

    // 驗證重連配置
    result = validate_reconnect_config(&g_config.reconnect);
    if (result != DMS_SUCCESS) {
        return result;
    }

    return DMS_SUCCESS;
}

void dms_config_cleanup(void) {
    if (g_config_initialized) {
        DMS_LOG_INFO("Cleaning up DMS configuration");
        memset(&g_config, 0, sizeof(g_config));
        g_config_initialized = false;
    }
}

/*-----------------------------------------------------------*/
/* 內部函數實作 */

static void load_default_aws_iot_config(dms_aws_iot_config_t* config) {
    // 從原本的 demo_config.h 常數載入
    strncpy(config->aws_endpoint, "apexd90h2t5wg-ats.iot.eu-central-1.amazonaws.com",
            sizeof(config->aws_endpoint) - 1);
    strncpy(config->client_id, "benq-dms-test-ABA1AE692AAE",
            sizeof(config->client_id) - 1);
    strncpy(config->ca_cert_path, "/etc/dms-client/rootCA.pem",
            sizeof(config->ca_cert_path) - 1);
    strncpy(config->client_cert_path, "/etc/dms-client/dms_pem.crt",
            sizeof(config->client_cert_path) - 1);
    strncpy(config->private_key_path, "/etc/dms-client/dms_private.pem.key",
            sizeof(config->private_key_path) - 1);

    config->mqtt_port = 8883;
    config->keep_alive_seconds = 60;
    config->connack_recv_timeout_ms = 1000;
    config->process_loop_timeout_ms = 1000;
    config->network_buffer_size = 2048;
    config->transport_timeout_ms = 5000;
}

static void load_default_api_config(dms_api_config_t* config) {
    // 從原本的 dms_api_client.h 常數載入
    strncpy(config->base_url, "https://dms-test.benq.com/api/",
            sizeof(config->base_url) - 1);
    strncpy(config->product_key, "DMS_Client_LINUX_APP_wvUVTQouuAMjriK5Vr7dO8ZIUkWOZ5wa",
            sizeof(config->product_key) - 1);
    strncpy(config->product_type, "instashow",
            sizeof(config->product_type) - 1);
    strncpy(config->user_agent, "DMS-Client/1.1.0",
            sizeof(config->user_agent) - 1);

    config->timeout_ms = 5000;
    config->max_retries = 3;
}

static void load_default_reconnect_config(dms_reconnect_config_t* config) {
    // 從原本的 dms_client.c 常數載入
    config->max_retry_attempts = 10;     // MAX_RETRY_ATTEMPTS
    config->base_delay_seconds = 2;      // RETRY_BACKOFF_BASE_SECONDS
    config->max_delay_seconds = 300;     // RETRY_BACKOFF_MAX_SECONDS
    config->shadow_get_timeout_ms = 10000;
    config->enable_exponential_backoff = true;
}

static dms_result_t validate_aws_iot_config(const dms_aws_iot_config_t* config) {
    if (!config) {
        return DMS_ERROR_INVALID_PARAMETER;  // ✅ 使用正確的錯誤碼
    }

    if (strlen(config->aws_endpoint) == 0) {
        DMS_LOG_ERROR("AWS IoT endpoint not configured");
        return DMS_ERROR_UCI_CONFIG_FAILED;  // ✅ 使用 demo_config.h 中存在的錯誤碼
    }

    if (strlen(config->client_id) == 0) {
        DMS_LOG_ERROR("Client ID not configured");
        return DMS_ERROR_UCI_CONFIG_FAILED;  // ✅ 使用 demo_config.h 中存在的錯誤碼
    }

    if (config->mqtt_port == 0) {
        DMS_LOG_ERROR("MQTT port not configured");
        return DMS_ERROR_UCI_CONFIG_FAILED;  // ✅ 使用 demo_config.h 中存在的錯誤碼
    }

    return DMS_SUCCESS;
}

static dms_result_t validate_api_config(const dms_api_config_t* config) {
    if (!config) {
        return DMS_ERROR_INVALID_PARAMETER;  // ✅ 使用正確的錯誤碼
    }

    if (strlen(config->base_url) == 0) {
        DMS_LOG_ERROR("API base URL not configured");
        return DMS_ERROR_UCI_CONFIG_FAILED;  // ✅ 使用 demo_config.h 中存在的錯誤碼
    }

    if (strlen(config->product_key) == 0) {
        DMS_LOG_ERROR("Product key not configured");
        return DMS_ERROR_UCI_CONFIG_FAILED;  // ✅ 使用 demo_config.h 中存在的錯誤碼
    }

    return DMS_SUCCESS;
}

static dms_result_t validate_reconnect_config(const dms_reconnect_config_t* config) {
    if (!config) {
        return DMS_ERROR_INVALID_PARAMETER;  // ✅ 使用正確的錯誤碼
    }

    if (config->max_retry_attempts == 0) {
        DMS_LOG_ERROR("Max retry attempts must be greater than 0");
        return DMS_ERROR_UCI_CONFIG_FAILED;  // ✅ 使用 demo_config.h 中存在的錯誤碼
    }

    if (config->base_delay_seconds == 0) {
        DMS_LOG_ERROR("Base delay must be greater than 0");
        return DMS_ERROR_UCI_CONFIG_FAILED;  // ✅ 使用 demo_config.h 中存在的錯誤碼
    }

    return DMS_SUCCESS;
}

