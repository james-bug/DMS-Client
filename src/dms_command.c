
/*
 * DMS Command Processing Module Implementation
 *
 * 這個檔案包含從 dms_client.c 提取的命令處理功能：
 * - parseShadowDelta() 函數邏輯 → dms_command_parse_shadow_delta()
 * - handleDMSCommand() 函數邏輯 → dms_command_execute()
 *
 * 所有函數邏輯與原始程式碼完全相同，只是重新組織結構。
 */

#include "dms_command.h"
#include "dms_shadow.h"      // 用於調用 reset 和 report 函數

/* AWS IoT 和 JSON 相關 - 與原始程式碼相同 */
#include "core_json.h"

/* 系統標頭檔 - 與原始程式碼相同 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 條件編譯 - 與原始程式碼相同 */
#ifdef DMS_API_ENABLED
#include "dms_api_client.h"
#endif

#ifdef BCML_MIDDLEWARE_ENABLED
#include "bcml_adapter.h"    // 用於 bcml_execute_wifi_control 聲明
#endif

/*-----------------------------------------------------------*/
/* 內部全域變數 */

static bool g_command_initialized = false;
static bcml_command_handler_t g_bcml_handler = NULL;

/* Shadow 介面函數指針 (用於依賴注入) */
static dms_result_t (*g_shadow_reset_desired)(const char* key) = NULL;
static dms_result_t (*g_shadow_report_result)(const char* key, bool success) = NULL;

/*-----------------------------------------------------------*/
/* 內部函數宣告 */

static dms_result_t execute_control_config_change_command(const dms_command_t* command);
static dms_result_t execute_upload_logs_command(void);
static dms_result_t execute_fw_upgrade_command(void);

/*-----------------------------------------------------------*/
/* 公開介面函數實作 */

/**
 * @brief 初始化命令處理模組
 */
dms_result_t dms_command_init(void)
{
    if (g_command_initialized) {
        DMS_LOG_WARN("Command module already initialized");
        return DMS_SUCCESS;
    }

    DMS_LOG_INFO("🔧 Initializing command processing module...");

    /* 重設內部狀態 */
    g_bcml_handler = NULL;
    g_shadow_reset_desired = NULL;
    g_shadow_report_result = NULL;

    g_command_initialized = true;
    DMS_LOG_INFO("✅ Command processing module initialized successfully");

    return DMS_SUCCESS;
}

/**
 * @brief 處理來自 Shadow Delta 的命令 - 完整流程
 */
dms_result_t dms_command_process_shadow_delta(const char* topic,
                                             const char* payload,
                                             size_t payload_len)
{
    if (!g_command_initialized) {
        DMS_LOG_ERROR("❌ Command module not initialized");
        return DMS_ERROR_INVALID_PARAMETER;
    }

    if (payload == NULL || payload_len == 0) {
        DMS_LOG_ERROR("❌ Invalid payload for command processing");
        return DMS_ERROR_INVALID_PARAMETER;
    }

    DMS_LOG_SHADOW("🔃 Processing Shadow delta command...");

    /* 步驟1：解析命令 - 與原始程式碼邏輯完全相同 */
    dms_command_t command;
    dms_result_t parse_result = dms_command_parse_shadow_delta(payload, payload_len, &command);

    if (parse_result != DMS_SUCCESS || command.type == DMS_CMD_NONE) {
        DMS_LOG_DEBUG("No valid command found in Shadow delta");
        return parse_result;
    }

    /* 步驟2：執行命令 - 與原始程式碼邏輯完全相同 */
    DMS_LOG_INFO("⚡ Executing DMS command: %s", command.key);
    dms_result_t exec_result = dms_command_execute(&command);

    /* 步驟3：重設 desired 狀態 - 委託給 Shadow 模組 */
    if (g_shadow_reset_desired != NULL) {
        dms_result_t reset_result = g_shadow_reset_desired(command.key);
        if (reset_result != DMS_SUCCESS) {
            DMS_LOG_WARN("⚠️ Failed to reset desired state for key: %s", command.key);
        }
    } else {
        DMS_LOG_WARN("⚠️ Shadow reset function not registered");
    }

    /* 步驟4：回報執行結果 - 委託給 Shadow 模組 */
    if (g_shadow_report_result != NULL) {
        bool success = (exec_result == DMS_SUCCESS);
        dms_result_t report_result = g_shadow_report_result(command.key, success);
        if (report_result != DMS_SUCCESS) {
            DMS_LOG_WARN("⚠️ Failed to report command result for key: %s", command.key);
        }
    } else {
        DMS_LOG_WARN("⚠️ Shadow report function not registered");
    }

    return exec_result;
}

/**
 * @brief 解析 Shadow Delta JSON - 從原始 parseShadowDelta() 函數提取
 */
dms_result_t dms_command_parse_shadow_delta(const char* payload,
                                           size_t payload_len,
                                           dms_command_t* command)
{
    if (payload == NULL || payload_len == 0 || command == NULL) {
        return DMS_ERROR_INVALID_PARAMETER;
    }

    /* JSON 解析變數 - 與原始程式碼完全相同 */
    JSONStatus_t jsonResult;
    char* valueStart;
    size_t valueLength;

    /* 驗證 JSON 格式 - 與原始程式碼邏輯完全相同 */
    jsonResult = JSON_Validate(payload, payload_len);
    if (jsonResult != JSONSuccess) {
        DMS_LOG_ERROR("❌ Invalid JSON in Shadow delta. JSON_Validate Error: %d", jsonResult);
        return DMS_ERROR_SHADOW_FAILURE;
    }

    DMS_LOG_DEBUG("📋 Parsing Shadow Delta JSON...");
    DMS_LOG_DEBUG("JSON Payload: %.*s", (int)payload_len, payload);

    /* 初始化命令結構 - 與原始程式碼完全相同 */
    memset(command, 0, sizeof(dms_command_t));
    command->type = DMS_CMD_NONE;
    command->timestamp = (uint32_t)time(NULL);

    /* 檢查 control-config-change - 與原始程式碼邏輯完全相同 */
    jsonResult = JSON_Search((char*)payload, payload_len,
                           JSON_QUERY_CONTROL_CONFIG, strlen(JSON_QUERY_CONTROL_CONFIG),
                           &valueStart, &valueLength);

    if (jsonResult == JSONSuccess && valueLength > 0) {
        command->type = DMS_CMD_CONTROL_CONFIG_CHANGE;
        command->value = (valueStart[0] == '1') ? 1 : 0;
        SAFE_STRNCPY(command->key, DMS_COMMAND_KEY_CONTROL_CONFIG, sizeof(command->key));
        DMS_LOG_INFO("🎯 Found control-config-change command: %d", command->value);
        return DMS_SUCCESS;
    }

    /* 檢查 upload_logs - 與原始程式碼邏輯完全相同 */
    jsonResult = JSON_Search((char*)payload, payload_len,
                           JSON_QUERY_UPLOAD_LOGS, strlen(JSON_QUERY_UPLOAD_LOGS),
                           &valueStart, &valueLength);

    if (jsonResult == JSONSuccess && valueLength > 0) {
        command->type = DMS_CMD_UPLOAD_LOGS;
        command->value = (valueStart[0] == '1') ? 1 : 0;
        SAFE_STRNCPY(command->key, DMS_COMMAND_KEY_UPLOAD_LOGS, sizeof(command->key));
        DMS_LOG_INFO("📤 Found upload_logs command: %d", command->value);
        return DMS_SUCCESS;
    }

    /* 檢查 fw_upgrade - 與原始程式碼邏輯完全相同 */
    jsonResult = JSON_Search((char*)payload, payload_len,
                           JSON_QUERY_FW_UPGRADE, strlen(JSON_QUERY_FW_UPGRADE),
                           &valueStart, &valueLength);

    if (jsonResult == JSONSuccess && valueLength > 0) {
        command->type = DMS_CMD_FW_UPGRADE;
        command->value = (valueStart[0] == '1') ? 1 : 0;
        SAFE_STRNCPY(command->key, DMS_COMMAND_KEY_FW_UPGRADE, sizeof(command->key));
        DMS_LOG_INFO("🔄 Found fw_upgrade command: %d", command->value);
        return DMS_SUCCESS;
    }

    /* 沒有找到任何命令 */
    DMS_LOG_DEBUG("No recognized command found in Shadow delta");
    return DMS_SUCCESS;  // 不是錯誤，只是沒有命令
}

/**
 * @brief 執行 DMS 命令 - 從原始 handleDMSCommand() 函數提取
 */
dms_result_t dms_command_execute(const dms_command_t* command)
{
    if (command == NULL) {
        return DMS_ERROR_INVALID_PARAMETER;
    }

    /* 檢查命令值 - 與原始程式碼邏輯相同 */
    if (command->value != 1) {
        DMS_LOG_WARN("⚠️ Command value is not 1, skipping execution");
        return DMS_ERROR_INVALID_PARAMETER;
    }

    DMS_LOG_INFO("🔧 Processing DMS command: %s (type: %d)", command->key, command->type);

    /* 根據命令類型執行 - 與原始程式碼邏輯完全相同 */
    switch (command->type) {
        case DMS_CMD_CONTROL_CONFIG_CHANGE:
            return execute_control_config_change_command(command);

        case DMS_CMD_UPLOAD_LOGS:
            return execute_upload_logs_command();

        case DMS_CMD_FW_UPGRADE:
            return execute_fw_upgrade_command();

        case DMS_CMD_NONE:
        default:
            DMS_LOG_ERROR("❌ Unknown DMS command type: %d", command->type);
            return DMS_ERROR_INVALID_PARAMETER;
    }
}

/**
 * @brief 註冊 BCML 命令處理器
 */
void dms_command_register_bcml_handler(bcml_command_handler_t handler)
{
    g_bcml_handler = handler;
    if (handler != NULL) {
        DMS_LOG_INFO("✅ BCML command handler registered");
    } else {
        DMS_LOG_WARN("⚠️ BCML command handler set to NULL");
    }
}

/**
 * @brief 註冊 Shadow 介面函數
 */
void dms_command_register_shadow_interface(
    dms_result_t (*reset_func)(const char* key),
    dms_result_t (*report_func)(const char* key, bool success))
{
    g_shadow_reset_desired = reset_func;
    g_shadow_report_result = report_func;

    if (reset_func != NULL && report_func != NULL) {
        DMS_LOG_INFO("✅ Shadow interface functions registered");
    } else {
        DMS_LOG_WARN("⚠️ Shadow interface functions partially registered");
    }
}

/**
 * @brief 清理命令處理模組
 */
void dms_command_cleanup(void)
{
    if (!g_command_initialized) {
        return;
    }

    DMS_LOG_INFO("🧹 Cleaning up command processing module...");

    g_bcml_handler = NULL;
    g_shadow_reset_desired = NULL;
    g_shadow_report_result = NULL;
    g_command_initialized = false;

    DMS_LOG_INFO("✅ Command processing module cleanup completed");
}

/*-----------------------------------------------------------*/
/* 內部函數實作 - 從原始 handleDMSCommand() 函數提取 */

/**
 * @brief 執行 control-config-change 命令
 */
static dms_result_t execute_control_config_change_command(const dms_command_t* command)
{
    DMS_LOG_INFO("📡 Processing WiFi control-config-change command...");

#ifdef DMS_API_ENABLED
    /* 使用實際的 DMS API 調用 - 與原始程式碼邏輯完全相同 */

    /* 獲取控制配置列表 */
    DMSControlConfig_t configs[10];
    int configCount = 0;
    DMSAPIResult_t apiResult = dms_api_control_config_list(
        CLIENT_IDENTIFIER, configs, 10, &configCount);

    if (apiResult == DMS_API_SUCCESS && configCount > 0) {
        DMS_LOG_INFO("✅ Control config retrieved: %d configurations", configCount);

        /* 執行所有控制配置 */
        bool allSuccess = true;
        for (int i = 0; i < configCount; i++) {
            /* 使用 BCML 處理器執行配置 */
            if (g_bcml_handler != NULL) {
                int execResult = g_bcml_handler(configs[i].item, configs[i].value);
                if (execResult != DMS_SUCCESS) {
                    DMS_LOG_ERROR("❌ Control failed for: %s", configs[i].item);
                    allSuccess = false;
                } else {
                    DMS_LOG_INFO("✅ Control successful for: %s", configs[i].item);
                }
            } else {
                DMS_LOG_WARN("⚠️ No BCML handler registered, simulating success");
            }
        }

        /* 回報每個控制的執行結果 - 與原始程式碼邏輯完全相同 */
        for (int i = 0; i < configCount; i++) {
            DMSControlResult_t controlResult = {
                .statusProgressId = configs[i].statusProgressId,
                .status = allSuccess ? 1 : 2,  // 1=successful, 2=failed
                .failedCode = "",
                .failedReason = ""
            };

            DMSAPIResult_t updateResult = dms_api_control_progress_update(
                CLIENT_IDENTIFIER, &controlResult, 1);

            if (updateResult == DMS_API_SUCCESS) {
                DMS_LOG_INFO("✅ Control progress reported for: %s", configs[i].item);
            } else {
                DMS_LOG_WARN("⚠️ Failed to report progress for: %s", configs[i].item);
            }
        }

        return allSuccess ? DMS_SUCCESS : DMS_ERROR_SHADOW_FAILURE;
    } else {
        DMS_LOG_ERROR("❌ Failed to get control config list: %d", apiResult);
        return DMS_ERROR_SHADOW_FAILURE;
    }

#else
    /* DMS API 未啟用時的模擬實作 - 與原始程式碼完全相同 */
    DMS_LOG_INFO("🎛️ Processing control-config-change command (simulation)...");
    DMS_LOG_INFO("✅ Control config change command processed (placeholder)");
    return DMS_SUCCESS;
#endif
}

/**
 * @brief 執行 upload_logs 命令
 */
static dms_result_t execute_upload_logs_command(void)
{
    DMS_LOG_INFO("📤 Processing upload_logs command...");

#ifdef DMS_API_ENABLED
    /* 實際的日誌上傳邏輯 - 與原始程式碼相同 */
    // TODO: 實作實際的日誌上傳功能
    DMS_LOG_INFO("✅ Upload logs command processed (placeholder)");
    return DMS_SUCCESS;
#else
    /* 模擬實作 - 與原始程式碼完全相同 */
    DMS_LOG_INFO("📤 Processing upload_logs command (simulation)...");
    DMS_LOG_INFO("✅ Upload logs command processed (placeholder)");
    return DMS_SUCCESS;
#endif
}

/**
 * @brief 執行 fw_upgrade 命令
 */
static dms_result_t execute_fw_upgrade_command(void)
{
    DMS_LOG_INFO("🔄 Processing fw_upgrade command...");

#ifdef DMS_API_ENABLED
    /* 實際的韌體更新邏輯 - 與原始程式碼相同 */
    // TODO: 實作實際的韌體更新功能
    DMS_LOG_INFO("✅ Firmware upgrade command processed (placeholder)");
    return DMS_SUCCESS;
#else
    /* 模擬實作 - 與原始程式碼完全相同 */
    DMS_LOG_INFO("🔄 Processing fw_upgrade command (simulation)...");
    DMS_LOG_INFO("✅ Firmware upgrade command processed (placeholder)");
    return DMS_SUCCESS;
#endif
}

