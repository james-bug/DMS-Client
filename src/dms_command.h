

/*
 * DMS Command Processing Module
 *
 * 這個模組提取自 dms_client.c 中的命令處理功能：
 * - parseShadowDelta() → dms_command_parse_shadow_delta()
 * - handleDMSCommand() → dms_command_execute()
 * - 與 dms_shadow.c 協作處理 resetDesiredState() 和 reportCommandResult()
 *
 * 功能完全保持不變，只是重新組織代碼結構。
 */

#ifndef DMS_COMMAND_H_
#define DMS_COMMAND_H_

/*-----------------------------------------------------------*/
/* 包含必要的標頭檔 */

#include "dms_config.h"
#include "dms_log.h"
#include "demo_config.h"    // 使用現有的錯誤碼和常數

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*-----------------------------------------------------------*/
/* 命令類型定義 - 使用 demo_config.h 中已定義的類型 */

/* 直接使用 demo_config.h 中已定義的類型，不重複定義 */
typedef DMSCommandType_t dms_command_type_t;    // 類型別名，方便使用
typedef DMSCommand_t dms_command_t;              // 結構別名，方便使用

/*-----------------------------------------------------------*/
/* BCML 適配器類型定義 */

/**
 * @brief BCML 命令處理器函數類型
 * 對應原始的 bcml_execute_wifi_control 函數簽名
 */
typedef int (*bcml_command_handler_t)(const char* item, const char* value);

/*-----------------------------------------------------------*/
/* 公開介面函數 */

/**
 * @brief 初始化命令處理模組
 *
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_command_init(void);

/**
 * @brief 處理來自 Shadow Delta 的命令
 *
 * 這個函數整合了原始的完整命令處理流程：
 * 1. parseShadowDelta() - 解析 JSON 命令
 * 2. handleDMSCommand() - 執行命令
 * 3. resetDesiredState() - 重設 desired 狀態 (委託給 dms_shadow)
 * 4. reportCommandResult() - 回報結果 (委託給 dms_shadow)
 *
 * @param topic Shadow 主題 (用於日誌記錄)
 * @param payload JSON payload
 * @param payload_len Payload 長度
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_command_process_shadow_delta(const char* topic,
                                             const char* payload,
                                             size_t payload_len);

/**
 * @brief 解析 Shadow Delta JSON
 *
 * 封裝原始的 parseShadowDelta() 函數，邏輯完全相同
 *
 * @param payload JSON payload
 * @param payload_len Payload 長度
 * @param command 輸出命令結構
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_command_parse_shadow_delta(const char* payload,
                                           size_t payload_len,
                                           dms_command_t* command);

/**
 * @brief 執行 DMS 命令
 *
 * 封裝原始的 handleDMSCommand() 函數，邏輯完全相同
 *
 * @param command 命令結構
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_command_execute(const dms_command_t* command);

/**
 * @brief 註冊 BCML 命令處理器
 *
 * 實現依賴注入，避免直接依賴 BCML 模組
 * 在 main() 中調用：dms_command_register_bcml_handler(bcml_execute_wifi_control);
 *
 * @param handler BCML 處理函數 (bcml_execute_wifi_control)
 */
void dms_command_register_bcml_handler(bcml_command_handler_t handler);

/**
 * @brief 清理命令處理模組
 */
void dms_command_cleanup(void);

/*-----------------------------------------------------------*/
/* 內部函數 (供 dms_shadow.c 調用) */

/**
 * @brief 註冊 Shadow 介面函數
 *
 * 讓命令模組可以調用 Shadow 模組的 reset 和 report 函數
 *
 * @param reset_func resetDesiredState 函數指針
 * @param report_func reportCommandResult 函數指針
 */
void dms_command_register_shadow_interface(
    dms_result_t (*reset_func)(const char* key),
    dms_result_t (*report_func)(const char* key, bool success)
);

#endif /* DMS_COMMAND_H_ */

