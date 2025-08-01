
/*
 * DMS Shadow Module Implementation
 *
 * This file contains the implementation of Shadow functionality extracted from dms_client.c
 * All functions maintain identical behavior to the original implementation.
 */

#include "dms_shadow.h"
#include "dms_command.h"

/* AWS IoT SDK includes - 與原始程式碼相同 */
#include "core_mqtt.h"
#include "core_json.h"

/* System includes - 與原始程式碼相同 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/sysinfo.h>

/* 需要引入 dms_aws_iot.h 來使用 dms_aws_iot_register_message_callback */
#include "dms_aws_iot.h"

/*-----------------------------------------------------------*/
/* 內部全域變數 */

/* Shadow 模組上下文 - 封裝原始的全域變數 */
static shadow_context_t g_shadow_context = { 0 };

/* Shadow 主題陣列 - 從原始程式碼提取 */
static const char* g_shadow_topics[SHADOW_MAX_TOPICS] = {
    SHADOW_UPDATE_ACCEPTED_TOPIC,
    SHADOW_UPDATE_REJECTED_TOPIC,
    SHADOW_UPDATE_DELTA_TOPIC,
    SHADOW_GET_ACCEPTED_TOPIC,
    SHADOW_GET_REJECTED_TOPIC
};

/*-----------------------------------------------------------*/
/* 內部函數宣告 */

static void shadow_message_handler(const char* topic, const char* payload, size_t payload_length);
static dms_result_t parse_device_bind_info(const char* payload, size_t payload_length, device_bind_info_t* bind_info);
static bool is_device_bound(const device_bind_info_t* bind_info);
static void update_system_stats(shadow_reported_state_t* state);
static uint32_t get_system_uptime(void);

/*-----------------------------------------------------------*/
/* 公開介面函數實作 */

/**
 * @brief 初始化 Shadow 模組
 */
dms_result_t dms_shadow_init(const mqtt_interface_t* mqtt_if)
{
    if (mqtt_if == NULL) {
        DMS_LOG_ERROR("❌ Invalid MQTT interface for Shadow initialization");
        return DMS_ERROR_INVALID_PARAMETER;
    }

    /* 儲存 MQTT 介面 */
    g_shadow_context.mqtt_interface = *mqtt_if;

    /* 初始化狀態 */
    memset(&g_shadow_context.reported_state, 0, sizeof(g_shadow_context.reported_state));
    memset(&g_shadow_context.bind_info, 0, sizeof(g_shadow_context.bind_info));

    g_shadow_context.initialized = true;
    g_shadow_context.get_pending = false;
    g_shadow_context.get_received = false;
    g_shadow_context.last_update_time = 0;
    g_shadow_context.message_callback = NULL;

    /* 註冊訊息處理器到 AWS IoT 模組 */
    dms_aws_iot_register_message_callback(shadow_message_handler);

       /* 🔥 新增：註冊 Shadow 介面到命令處理模組 */
    dms_command_register_shadow_interface(
        dms_shadow_reset_desired,
        dms_shadow_report_command_result
    );

    g_shadow_context.initialized = true;

    DMS_LOG_INFO("✅ Shadow module initialized successfully");
    return DMS_SUCCESS;
}

/**
 * @brief 開始 Shadow 服務
 */
dms_result_t dms_shadow_start(void)
{
    if (!g_shadow_context.initialized) {
        DMS_LOG_ERROR("❌ Shadow module not initialized");
        return DMS_ERROR_INVALID_PARAMETER;  // 使用正確的錯誤碼
    }

    /* 訂閱 Shadow 主題 - 與原始邏輯完全相同 */
    dms_result_t result = dms_shadow_subscribe_topics();
    if (result != DMS_SUCCESS) {
        DMS_LOG_ERROR("❌ Failed to subscribe to Shadow topics");
        return result;
    }

    /* 獲取 Shadow 文檔 - 與原始邏輯完全相同 */
    result = dms_shadow_get_document();
    if (result != DMS_SUCCESS) {
        DMS_LOG_ERROR("❌ Failed to get Shadow document");
        return result;
    }

    DMS_LOG_INFO("✅ Shadow service started successfully");
    return DMS_SUCCESS;
}

/**
 * @brief 訂閱所有 Shadow 主題
 *
 * 這個函數完全複製原始的 subscribeToShadowTopics() 邏輯
 */
dms_result_t dms_shadow_subscribe_topics(void)
{
    if (!g_shadow_context.initialized) {
        return DMS_ERROR_INVALID_PARAMETER;  // 使用正確的錯誤碼
    }

    DMS_LOG_SHADOW("📡 Subscribing to Shadow topics...");

    /* 訂閱所有 Shadow 主題 - 與原始程式碼邏輯完全相同 */
    for (int i = 0; i < SHADOW_MAX_TOPICS; i++) {
        dms_result_t result = g_shadow_context.mqtt_interface.subscribe(
            g_shadow_topics[i],
            shadow_message_handler
        );

        if (result != DMS_SUCCESS) {
            DMS_LOG_ERROR("❌ Failed to subscribe to topic: %s", g_shadow_topics[i]);
            return result;
        }

        DMS_LOG_DEBUG("✓ Subscribed to: %s", g_shadow_topics[i]);
    }

    /* 等待訂閱確認 - 與原始程式碼相同 */
    DMS_LOG_DEBUG("⏳ Waiting for subscription confirmations...");
    for (int i = 0; i < 10; i++) {
        g_shadow_context.mqtt_interface.process_loop(300);  // 300ms
        usleep(300000);
    }

    DMS_LOG_SHADOW("✅ Shadow topics subscription completed");
    return DMS_SUCCESS;
}

/**
 * @brief 請求獲取 Shadow 文檔
 *
 * 這個函數完全複製原始的 getShadowDocument() 邏輯
 */
dms_result_t dms_shadow_get_document(void)
{
    if (!g_shadow_context.initialized) {
        return DMS_ERROR_INVALID_PARAMETER;  // 使用正確的錯誤碼
    }

    DMS_LOG_SHADOW("📨 Requesting Shadow document...");

    /* 標記 Shadow Get 請求開始 - 與原始程式碼相同 */
    g_shadow_context.get_pending = true;
    g_shadow_context.get_received = false;

    /* 發送 Shadow Get 請求 - 與原始程式碼邏輯完全相同 */
    dms_result_t result = g_shadow_context.mqtt_interface.publish(
        SHADOW_GET_TOPIC,
        SHADOW_GET_REQUEST_PAYLOAD,
        strlen(SHADOW_GET_REQUEST_PAYLOAD)
    );

    if (result != DMS_SUCCESS) {
        DMS_LOG_ERROR("❌ Failed to send Shadow Get request");
        g_shadow_context.get_pending = false;
        return result;
    }

    DMS_LOG_SHADOW("✅ Shadow Get request sent successfully");
    return DMS_SUCCESS;
}

/**
 * @brief 等待 Shadow Get 回應
 *
 * 這個函數完全複製原始的 waitForShadowGetResponse() 邏輯
 */
dms_result_t dms_shadow_wait_get_response(uint32_t timeout_ms)
{
    if (!g_shadow_context.initialized) {
        return DMS_ERROR_INVALID_PARAMETER;  // 使用正確的錯誤碼
    }

    uint32_t start_time = (uint32_t)time(NULL);
    uint32_t current_time;
    uint32_t elapsed_seconds = 0;

    DMS_LOG_DEBUG("⏳ Waiting for Shadow Get response (timeout: %u ms)...", timeout_ms);

    /* 等待迴圈 - 與原始程式碼邏輯完全相同 */
    while (g_shadow_context.get_pending && !g_shadow_context.get_received &&
           elapsed_seconds * 1000 < timeout_ms) {

        /* 處理 MQTT 事件 - 與原始程式碼相同 */
        dms_result_t result = g_shadow_context.mqtt_interface.process_loop(100);
        if (result != DMS_SUCCESS) {
            DMS_LOG_ERROR("❌ MQTT process loop failed while waiting for Shadow Get");
            g_shadow_context.get_pending = false;
            return DMS_ERROR_MQTT_FAILURE;
        }

        /* 更新經過時間 - 與原始程式碼相同 */
        current_time = (uint32_t)time(NULL);
        elapsed_seconds = current_time - start_time;

        /* 每 2 秒顯示等待狀態 - 與原始程式碼相同 */
        if (elapsed_seconds > 0 && elapsed_seconds % 2 == 0) {
            DMS_LOG_DEBUG("   ⏳ Still waiting... (%u/%u seconds)", elapsed_seconds, timeout_ms/1000);
        }

        usleep(100000); // 100ms
    }

    /* 檢查結果 - 與原始程式碼邏輯完全相同 */
    if (g_shadow_context.get_received) {
        DMS_LOG_SHADOW("✅ Shadow Get response received successfully");
        g_shadow_context.get_pending = false;
        return DMS_SUCCESS;
    }

    if (elapsed_seconds * 1000 >= timeout_ms) {
        DMS_LOG_WARN("⏰ Shadow Get request timed out after %u seconds", elapsed_seconds);
    } else {
        DMS_LOG_ERROR("❌ Shadow Get response not received");
    }

    g_shadow_context.get_pending = false;
    return DMS_ERROR_TIMEOUT;
}

/**
 * @brief 更新 Shadow reported 狀態
 *
 * 這個函數完全複製原始的 publishShadowUpdate() 邏輯
 */
dms_result_t dms_shadow_update_reported(const shadow_reported_state_t* state)
{
    if (!g_shadow_context.initialized) {
        return DMS_ERROR_INVALID_PARAMETER;  // 使用正確的錯誤碼
    }

    char payload[512];
    const shadow_reported_state_t* update_state;

    /* 如果沒有提供狀態，則更新並使用內部狀態 */
    if (state == NULL) {
        update_system_stats(&g_shadow_context.reported_state);
        update_state = &g_shadow_context.reported_state;
    } else {
        update_state = state;
    }

    /* 準備 Shadow JSON 訊息 - 與原始程式碼格式完全相同 */
    snprintf(payload, sizeof(payload),
             SHADOW_REPORTED_JSON_TEMPLATE,
             update_state->connected ? "true" : "false",
             update_state->status,
             update_state->uptime,
             update_state->lastHeartbeat,
             update_state->firmwareVersion,
             update_state->deviceType,
             update_state->cpuUsage,
             update_state->memoryUsage,
             update_state->networkBytesSent,
             update_state->networkBytesReceived);

    DMS_LOG_SHADOW("📤 Publishing Shadow update...");
    DMS_LOG_DEBUG("Payload: %s", payload);

    /* 發布訊息 - 與原始程式碼邏輯完全相同 */
    dms_result_t result = g_shadow_context.mqtt_interface.publish(
        SHADOW_UPDATE_TOPIC,
        payload,
        strlen(payload)
    );

    if (result != DMS_SUCCESS) {
        DMS_LOG_ERROR("❌ Failed to publish Shadow update");
        return result;
    }

    g_shadow_context.last_update_time = (uint32_t)time(NULL);
    DMS_LOG_SHADOW("✅ Shadow update published successfully");
    return DMS_SUCCESS;
}

/**
 * @brief 重設 Shadow desired 狀態中的指定鍵
 *
 * 這個函數完全複製原始的 resetDesiredState() 邏輯
 */
dms_result_t dms_shadow_reset_desired(const char* key)
{
    if (!g_shadow_context.initialized || key == NULL) {
        return DMS_ERROR_INVALID_PARAMETER;
    }

    char payload[256];

    /* 準備重設 JSON 訊息 - 與原始程式碼格式完全相同 */
    snprintf(payload, sizeof(payload),
             "{"
             "\"state\": {"
             "\"desired\": {"
             "\"%s\": null"
             "}"
             "}"
             "}",
             key);

    DMS_LOG_DEBUG("🔄 Resetting desired state for key: %s", key);
    DMS_LOG_DEBUG("Reset payload: %s", payload);

    /* 發布重設訊息 - 與原始程式碼邏輯完全相同 */
    dms_result_t result = g_shadow_context.mqtt_interface.publish(
        SHADOW_UPDATE_TOPIC,
        payload,
        strlen(payload)
    );

    if (result != DMS_SUCCESS) {
        DMS_LOG_ERROR("❌ Failed to reset desired state for key: %s", key);
        return result;
    }

    DMS_LOG_DEBUG("✅ Desired state reset successfully for key: %s", key);
    return DMS_SUCCESS;
}

/**
 * @brief 回報命令執行結果到 Shadow
 *
 * 這個函數完全複製原始的 reportCommandResult() 邏輯
 */
dms_result_t dms_shadow_report_command_result(const char* command_key, bool result)
{
    if (!g_shadow_context.initialized || command_key == NULL) {
        return DMS_ERROR_INVALID_PARAMETER;
    }

    char payload[256];
    const char* result_str = result ? "success" : "failed";

    /* 準備結果報告 JSON 訊息 - 與原始程式碼格式完全相同 */
    snprintf(payload, sizeof(payload),
             "{"
             "\"state\": {"
             "\"reported\": {"
             "\"%s_result\": \"%s\","
             "\"%s_timestamp\": %u"
             "}"
             "}"
             "}",
             command_key, result_str,
             command_key, (uint32_t)time(NULL));

    DMS_LOG_DEBUG("📊 Reporting command result: %s = %s", command_key, result_str);

    /* 發布結果報告 - 與原始程式碼邏輯完全相同 */
    dms_result_t publish_result = g_shadow_context.mqtt_interface.publish(
        SHADOW_UPDATE_TOPIC,
        payload,
        strlen(payload)
    );

    if (publish_result != DMS_SUCCESS) {
        DMS_LOG_ERROR("❌ Failed to report command result for: %s", command_key);
        return publish_result;
    }

    DMS_LOG_DEBUG("✅ Command result reported successfully: %s = %s", command_key, result_str);
    return DMS_SUCCESS;
}

/**
 * @brief 註冊 Shadow 訊息回調函數
 */
void dms_shadow_register_message_callback(shadow_message_callback_t callback)
{
    g_shadow_context.message_callback = callback;
    DMS_LOG_DEBUG("✅ Shadow message callback registered");
}

/**
 * @brief 檢查設備是否已綁定到 DMS Server
 */
bool dms_shadow_is_device_bound(void)
{
    return is_device_bound(&g_shadow_context.bind_info);
}

/**
 * @brief 獲取設備綁定資訊
 */
const device_bind_info_t* dms_shadow_get_bind_info(void)
{
    return &g_shadow_context.bind_info;
}

/**
 * @brief 獲取當前報告狀態
 */
const shadow_reported_state_t* dms_shadow_get_reported_state(void)
{
    return &g_shadow_context.reported_state;
}

/**
 * @brief 檢查 Shadow Get 是否完成
 */
bool dms_shadow_is_get_completed(void)
{
    return !g_shadow_context.get_pending && g_shadow_context.get_received;
}

/**
 * @brief 更新系統狀態資訊
 */
void dms_shadow_update_system_stats(void)
{
    update_system_stats(&g_shadow_context.reported_state);
}

/**
 * @brief 清理 Shadow 模組
 */
void dms_shadow_cleanup(void)
{
    memset(&g_shadow_context, 0, sizeof(g_shadow_context));
    DMS_LOG_INFO("✅ Shadow module cleaned up");
}

/*-----------------------------------------------------------*/
/* 內部函數實作 */

/**
 * @brief Shadow 訊息處理器
 *
 * 這個函數完全複製原始 eventCallback() 中的 Shadow 處理邏輯
 */
static void shadow_message_handler(const char* topic, const char* payload, size_t payload_length)
{
    if (topic == NULL || payload == NULL || payload_length == 0) {
        return;
    }

    DMS_LOG_SHADOW("📨 Shadow message received from topic: %s", topic);
    DMS_LOG_DEBUG("Payload length: %zu", payload_length);

    /* 主題匹配分析 - 與原始程式碼邏輯完全相同 */
    bool isUpdateAccepted = (strstr(topic, "/shadow/update/accepted") != NULL);
    bool isUpdateRejected = (strstr(topic, "/shadow/update/rejected") != NULL);
    bool isUpdateDelta = (strstr(topic, "/shadow/update/delta") != NULL);
    bool isGetAccepted = (strstr(topic, "/shadow/get/accepted") != NULL);
    bool isGetRejected = (strstr(topic, "/shadow/get/rejected") != NULL);

    DMS_LOG_DEBUG("🔍 Topic matching analysis:");
    DMS_LOG_DEBUG("   update/accepted: %s", isUpdateAccepted ? "✅ MATCH" : "❌ no match");
    DMS_LOG_DEBUG("   update/rejected: %s", isUpdateRejected ? "✅ MATCH" : "❌ no match");
    DMS_LOG_DEBUG("   update/delta: %s", isUpdateDelta ? "✅ MATCH" : "❌ no match");
    DMS_LOG_DEBUG("   get/accepted: %s", isGetAccepted ? "✅ MATCH" : "❌ no match");
    DMS_LOG_DEBUG("   get/rejected: %s", isGetRejected ? "✅ MATCH" : "❌ no match");

    /* 處理不同類型的 Shadow 訊息 - 與原始程式碼邏輯完全相同 */
    if (isUpdateAccepted) {
        DMS_LOG_SHADOW("🔄 Shadow update accepted");
    }
    else if (isUpdateRejected) {
        DMS_LOG_ERROR("❌ Shadow update rejected");
    }
 
    else if (isUpdateDelta) {
    	DMS_LOG_SHADOW("🔃 Shadow delta received - processing command directly...");

    	/* 🔥 新方式：直接調用命令處理模組 */
    	dms_result_t cmd_result = dms_command_process_shadow_delta(topic, payload, payload_length);
    
    	if (cmd_result == DMS_SUCCESS) {
        	DMS_LOG_SHADOW("✅ Shadow delta command processed successfully");
    	} else {
        	DMS_LOG_ERROR("❌ Failed to process Shadow delta command: %d", cmd_result);
    	}
    }	 		


    else if (isGetAccepted) {
        DMS_LOG_SHADOW("✅ Shadow get accepted - processing device binding info");

        /* 解析 Shadow 文檔並檢查綁定狀態 - 與原始程式碼邏輯完全相同 */
        dms_result_t parseResult = parse_device_bind_info(
            payload,
            payload_length,
            &g_shadow_context.bind_info
        );

        if (parseResult == DMS_SUCCESS) {
            if (is_device_bound(&g_shadow_context.bind_info)) {
                DMS_LOG_INFO("🎯 Device is bound to DMS Server");
                DMS_LOG_INFO("   Company: %s (ID: %s)",
                           g_shadow_context.bind_info.companyName,
                           g_shadow_context.bind_info.companyId);
                DMS_LOG_INFO("   Device: %s (Added by: %s)",
                           g_shadow_context.bind_info.deviceName,
                           g_shadow_context.bind_info.addedBy);
            } else {
                DMS_LOG_WARN("⚠️ Device is not bound to DMS Server");
                DMS_LOG_INFO("   Registration required for DMS functionality");
                /* TODO: 觸發 DMS Server 註冊流程 */
            }
        } else {
            DMS_LOG_WARN("⚠️ Failed to parse bind info from Shadow Get response");
        }

        /* 標記 Shadow Get 已接收 - 與原始程式碼相同 */
        g_shadow_context.get_received = true;
        g_shadow_context.get_pending = false;
        DMS_LOG_DEBUG("🔔 Shadow Get status updated: received=true, pending=false");
    }
    else if (isGetRejected) {
        DMS_LOG_ERROR("❌ Shadow get rejected");

        /* 標記 Shadow Get 失敗 - 與原始程式碼相同 */
        g_shadow_context.get_received = false;
        g_shadow_context.get_pending = false;
        DMS_LOG_DEBUG("🔔 Shadow Get status updated: received=false, pending=false");
    }
    else {
        DMS_LOG_WARN("❓ Unknown shadow topic or non-shadow message");
        DMS_LOG_DEBUG("   Full topic: %s", topic);
    }
}

/**
 * @brief 解析設備綁定資訊
 *
 * 這個函數完全複製原始的 parseDeviceBindInfo() 邏輯
 */
static dms_result_t parse_device_bind_info(const char* payload, size_t payload_length, device_bind_info_t* bind_info)
{
    if (payload == NULL || bind_info == NULL || payload_length == 0) {
        return DMS_ERROR_INVALID_PARAMETER;
    }

    /* 初始化綁定資訊 */
    memset(bind_info, 0, sizeof(device_bind_info_t));
    bind_info->bound = false;

    /* 驗證 JSON 格式 - 與原始程式碼相同 */
    JSONStatus_t jsonResult = JSON_Validate(payload, payload_length);
    if (jsonResult != JSONSuccess) {
        DMS_LOG_ERROR("❌ Invalid JSON format in bind info. Error: %d", jsonResult);
        return DMS_ERROR_SHADOW_FAILURE;
    }

    DMS_LOG_DEBUG("📋 Parsing device bind info JSON...");

    /* 尋找 state.reported.info 路徑 - 與原始程式碼邏輯完全相同 */
    char* valueStart;
    size_t valueLength;

    /* 查找 "state" */
    jsonResult = JSON_Search((char*)payload, payload_length, "state", 5, &valueStart, &valueLength);
    if (jsonResult != JSONSuccess) {
        DMS_LOG_DEBUG("No 'state' found in JSON, device not bound");
        return DMS_SUCCESS;  /* 不算錯誤，只是沒有綁定資訊 */
    }

    /* 在 state 中查找 "reported" */
    jsonResult = JSON_Search(valueStart, valueLength, "reported", 8, &valueStart, &valueLength);
    if (jsonResult != JSONSuccess) {
        DMS_LOG_DEBUG("No 'reported' found in state, device not bound");
        return DMS_SUCCESS;
    }

    /* 在 reported 中查找 "info" */
    jsonResult = JSON_Search(valueStart, valueLength, "info", 4, &valueStart, &valueLength);
    if (jsonResult != JSONSuccess) {
        DMS_LOG_DEBUG("No 'info' found in reported, device not bound");
        return DMS_SUCCESS;
    }

    /* 解析 info 中的各個欄位 - 與原始程式碼邏輯完全相同 */
    char* fieldStart;
    size_t fieldLength;

    /* 解析 company_name */
    jsonResult = JSON_Search(valueStart, valueLength, "company_name", 12, &fieldStart, &fieldLength);
    if (jsonResult == JSONSuccess && fieldLength > 0 && fieldLength < sizeof(bind_info->companyName)) {
        /* 移除引號並複製 */
        if (fieldStart[0] == '"' && fieldStart[fieldLength-1] == '"') {
            fieldStart++;
            fieldLength -= 2;
        }
        strncpy(bind_info->companyName, fieldStart, fieldLength);
        bind_info->companyName[fieldLength] = '\0';
    }

    /* 解析 added_by */
    jsonResult = JSON_Search(valueStart, valueLength, "added_by", 8, &fieldStart, &fieldLength);
    if (jsonResult == JSONSuccess && fieldLength > 0 && fieldLength < sizeof(bind_info->addedBy)) {
        if (fieldStart[0] == '"' && fieldStart[fieldLength-1] == '"') {
            fieldStart++;
            fieldLength -= 2;
        }
        strncpy(bind_info->addedBy, fieldStart, fieldLength);
        bind_info->addedBy[fieldLength] = '\0';
    }

    /* 解析 device_name */
    jsonResult = JSON_Search(valueStart, valueLength, "device_name", 11, &fieldStart, &fieldLength);
    if (jsonResult == JSONSuccess && fieldLength > 0 && fieldLength < sizeof(bind_info->deviceName)) {
        if (fieldStart[0] == '"' && fieldStart[fieldLength-1] == '"') {
            fieldStart++;
            fieldLength -= 2;
        }
        strncpy(bind_info->deviceName, fieldStart, fieldLength);
        bind_info->deviceName[fieldLength] = '\0';
    }

    /* 解析 company_id */
    jsonResult = JSON_Search(valueStart, valueLength, "company_id", 10, &fieldStart, &fieldLength);
    if (jsonResult == JSONSuccess && fieldLength > 0 && fieldLength < sizeof(bind_info->companyId)) {
        if (fieldStart[0] == '"' && fieldStart[fieldLength-1] == '"') {
            fieldStart++;
            fieldLength -= 2;
        }
        strncpy(bind_info->companyId, fieldStart, fieldLength);
        bind_info->companyId[fieldLength] = '\0';
    }

    /* 檢查是否所有必要欄位都存在 */
    bind_info->bound = (strlen(bind_info->companyName) > 0 &&
                       strlen(bind_info->companyId) > 0 &&
                       strlen(bind_info->deviceName) > 0 &&
                       strlen(bind_info->addedBy) > 0);

    DMS_LOG_DEBUG("📋 Bind info parsing completed: bound=%s", bind_info->bound ? "true" : "false");
    return DMS_SUCCESS;
}

/**
 * @brief 檢查設備是否已綁定
 *
 * 這個函數完全複製原始的 isDeviceBound() 邏輯
 */
static bool is_device_bound(const device_bind_info_t* bind_info)
{
    if (bind_info == NULL) {
        return false;
    }

    /* 檢查所有必要欄位是否都有值 - 與原始程式碼邏輯完全相同 */
    return (bind_info->bound &&
            strlen(bind_info->companyName) > 0 &&
            strlen(bind_info->companyId) > 0 &&
            strlen(bind_info->deviceName) > 0 &&
            strlen(bind_info->addedBy) > 0);
}

/**
 * @brief 更新系統狀態資訊
 *
 * 這個函數完全複製原始的 updateSystemStats() 邏輯
 */
static void update_system_stats(shadow_reported_state_t* state)
{
    struct sysinfo info;

    if (state == NULL) {
        return;
    }

    /* 更新基本資訊 - 與原始程式碼完全相同 */
    strncpy(state->deviceId, CLIENT_IDENTIFIER, sizeof(state->deviceId) - 1);
    state->deviceId[sizeof(state->deviceId) - 1] = '\0';

    strncpy(state->deviceType, "instashow", sizeof(state->deviceType) - 1);
    state->deviceType[sizeof(state->deviceType) - 1] = '\0';

    strncpy(state->firmwareVersion, "V1.0.0.1", sizeof(state->firmwareVersion) - 1);
    state->firmwareVersion[sizeof(state->firmwareVersion) - 1] = '\0';

    state->connected = true;
    strncpy(state->status, "online", sizeof(state->status) - 1);
    state->status[sizeof(state->status) - 1] = '\0';

    state->uptime = get_system_uptime();
    state->lastHeartbeat = (uint32_t)time(NULL);

    /* 獲取系統資訊 - 與原始程式碼邏輯完全相同 */
    if (sysinfo(&info) == 0) {
        /* CPU 使用率 (簡化版本) */
        state->cpuUsage = 0.0; // TODO: 實現真實的 CPU 使用率計算

        /* 記憶體使用率 */
        if (info.totalram > 0) {
            state->memoryUsage = (float)(info.totalram - info.freeram) / info.totalram * 100.0;
        }
    }

    /* 網路統計 (簡化版本) - 與原始程式碼相同 */
    state->networkBytesSent = 0;    // TODO: 從 /proc/net/dev 讀取
    state->networkBytesReceived = 0;
}

/**
 * @brief 獲取系統運行時間
 *
 * 這個函數完全複製原始的 getSystemUptime() 邏輯
 */
static uint32_t get_system_uptime(void)
{
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (uint32_t)info.uptime;
    }
    return 0;
}

/**
 * @brief 綁定檢查函數組合 - 按照規格要求的正確流程
 * 
 * 這個函數封裝了完整的設備綁定檢查流程：
 * 1. 獲取 Shadow 文檔
 * 2. 等待回應
 * 3. 解析綁定資訊
 * 4. 返回綁定狀態
 */
dms_result_t dms_shadow_check_device_binding(void)
{
    DMS_LOG_INFO("🔍 Checking device binding status via Shadow...");
    
    /* 步驟1：請求 Shadow 文檔 */
    dms_result_t result = dms_shadow_get_document();
    if (result != DMS_SUCCESS) {
        DMS_LOG_ERROR("❌ Failed to request Shadow document");
        return result;
    }
    
    /* 步驟2：等待 Shadow Get 回應 */
    DMS_LOG_DEBUG("⏳ Waiting for Shadow Get response...");
    result = dms_shadow_wait_get_response(SHADOW_GET_TIMEOUT_MS);
    if (result != DMS_SUCCESS) {
        DMS_LOG_WARN("❌ Failed to get Shadow response: %d", result);
        return result;
    }
    
    /* 步驟3：檢查綁定狀態（已在 shadow_message_handler 中解析） */
    if (dms_shadow_is_device_bound()) {
        DMS_LOG_INFO("✅ Device is bound to DMS Server");
        const device_bind_info_t* bind_info = dms_shadow_get_bind_info();
        DMS_LOG_INFO("   Company: %s (ID: %s)", bind_info->companyName, bind_info->companyId);
        DMS_LOG_INFO("   Device: %s (Added by: %s)", bind_info->deviceName, bind_info->addedBy);
        return DMS_SUCCESS;
    } else {
        DMS_LOG_WARN("⚠️ Device is not bound to DMS Server");
        DMS_LOG_INFO("   Registration will be required for DMS functionality");
        return DMS_ERROR_DEVICE_NOT_BOUND;  // 新的錯誤碼，表示未綁定
    }
}

