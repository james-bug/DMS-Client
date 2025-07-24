
/*
 * DMS Shadow Module
 *
 * This module encapsulates AWS IoT Device Shadow functionality extracted from dms_client.c
 * All functions maintain identical behavior to the original implementation.
 *
 * Functions extracted and their mappings:
 * - subscribeToShadowTopics()  → dms_shadow_subscribe_topics()
 * - getShadowDocument()        → dms_shadow_get_document()
 * - publishShadowUpdate()      → dms_shadow_update_reported()
 * - waitForShadowGetResponse() → dms_shadow_wait_get_response()
 * - parseShadowDelta()         → 將移到 dms_command 模組
 * - parseDeviceBindInfo()      → 內部函數
 * - eventCallback() Shadow 處理部分 → shadow_message_handler()
 */

#ifndef DMS_SHADOW_H_
#define DMS_SHADOW_H_

/*-----------------------------------------------------------*/
/* 包含必要的標頭檔 */

#include "dms_config.h"
#include "dms_log.h"
#include "dms_aws_iot.h"  // 為了使用 mqtt_interface_t

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/*-----------------------------------------------------------*/
/* 常數定義 - 使用 demo_config.h 中的定義，避免重複 */

/* Shadow 主題定義 - 直接使用 demo_config.h 中的定義 */
/* 以下常數已在 demo_config.h 中定義，無需重複定義：
 * - SHADOW_UPDATE_TOPIC
 * - SHADOW_UPDATE_ACCEPTED_TOPIC
 * - SHADOW_UPDATE_REJECTED_TOPIC
 * - SHADOW_UPDATE_DELTA_TOPIC
 * - SHADOW_GET_TOPIC
 * - SHADOW_GET_ACCEPTED_TOPIC
 * - SHADOW_GET_REJECTED_TOPIC
 * - SHADOW_GET_TIMEOUT_MS
 * - SHADOW_REPORTED_JSON_TEMPLATE
 */

/* Shadow 模組專用常數 */
#define SHADOW_MAX_TOPICS                  ( 5U )
#define SHADOW_GET_REQUEST_PAYLOAD         "{}"

/*-----------------------------------------------------------*/
/* 類型定義 - 從 dms_client.c 提取現有結構 */

/**
 * @brief Shadow 主題回調函數類型
 * 用於處理從不同 Shadow 主題接收到的訊息
 */
typedef void (*shadow_message_callback_t)(const char* topic,
                                         const char* payload,
                                         size_t payload_length);

/**
 * @brief Shadow Reported 狀態結構 - 從 dms_client.c 提取
 * 與原始 ShadowReportedState_t 完全相同
 */
typedef struct {
    char deviceId[64];
    char deviceType[32];
    char firmwareVersion[32];
    bool connected;
    char status[16];  // "online", "offline", "maintenance"
    uint32_t uptime;
    uint32_t lastHeartbeat;
    float cpuUsage;
    float memoryUsage;
    uint32_t networkBytesSent;
    uint32_t networkBytesReceived;
} shadow_reported_state_t;

/**
 * @brief 設備綁定資訊結構 - 從 dms_client.c 提取
 * 與原始 DeviceBindInfo_t 完全相同
 */
typedef struct {
    char companyName[128];
    char addedBy[64];
    char deviceName[128];
    char companyId[16];
    bool bound;
} device_bind_info_t;

/**
 * @brief Shadow 模組內部狀態
 */
typedef struct {
    mqtt_interface_t mqtt_interface;    // 注入的 MQTT 介面
    shadow_reported_state_t reported_state;  // 當前報告狀態
    device_bind_info_t bind_info;       // 設備綁定資訊
    bool initialized;                   // 初始化標誌
    bool get_pending;                   // Shadow Get 請求等待中
    bool get_received;                  // Shadow Get 回應已接收
    uint32_t last_update_time;          // 最後更新時間
    shadow_message_callback_t message_callback;  // 外部訊息回調
} shadow_context_t;

/*-----------------------------------------------------------*/
/* 公開介面函數 */

/**
 * @brief 初始化 Shadow 模組
 *
 * 這個函數會：
 * 1. 儲存注入的 MQTT 介面
 * 2. 初始化內部狀態
 * 3. 準備 Shadow 主題和狀態管理
 *
 * @param mqtt_if MQTT 介面結構指針 (從 dms_aws_iot_get_interface() 獲得)
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_shadow_init(const mqtt_interface_t* mqtt_if);

/**
 * @brief 開始 Shadow 服務
 *
 * 這個函數封裝了原始的：
 * - subscribeToShadowTopics()
 * - getShadowDocument()
 *
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_shadow_start(void);

/**
 * @brief 訂閱所有 Shadow 主題
 *
 * 封裝原始的 subscribeToShadowTopics() 函數
 * 訂閱 5 個 Shadow 主題：update/accepted, update/rejected, update/delta, get/accepted, get/rejected
 *
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_shadow_subscribe_topics(void);

/**
 * @brief 請求獲取 Shadow 文檔
 *
 * 封裝原始的 getShadowDocument() 函數
 * 發送 Shadow Get 請求到 AWS IoT
 *
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_shadow_get_document(void);

/**
 * @brief 等待 Shadow Get 回應
 *
 * 封裝原始的 waitForShadowGetResponse() 函數
 * 等待並處理 Shadow Get 回應，包含超時處理
 *
 * @param timeout_ms 超時時間（毫秒）
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_shadow_wait_get_response(uint32_t timeout_ms);

/**
 * @brief 更新 Shadow reported 狀態
 *
 * 封裝原始的 publishShadowUpdate() 函數
 * 發送系統狀態更新到 AWS IoT Device Shadow
 *
 * @param state 要更新的狀態結構，如果為 NULL 則使用內部狀態
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_shadow_update_reported(const shadow_reported_state_t* state);

/**
 * @brief 重設 Shadow desired 狀態中的指定鍵
 *
 * 封裝原始的 resetDesiredState() 函數
 * 將 desired 狀態中的指定鍵設為 null，避免重複處理命令
 *
 * @param key 要重設的鍵名
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_shadow_reset_desired(const char* key);

/**
 * @brief 回報命令執行結果到 Shadow
 *
 * 封裝原始的 reportCommandResult() 函數
 * 將命令執行結果更新到 Shadow reported 狀態
 *
 * @param command_key 命令鍵名
 * @param result 執行結果（成功/失敗）
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_shadow_report_command_result(const char* command_key, bool result);

/**
 * @brief 註冊 Shadow 訊息回調函數
 *
 * 允許其他模組（如 dms_command）註冊處理器來接收 Shadow 訊息
 * 特別是 Delta 訊息會轉發給命令處理模組
 *
 * @param callback 回調函數
 */
void dms_shadow_register_message_callback(shadow_message_callback_t callback);

/**
 * @brief 更新系統狀態資訊
 *
 * 封裝原始的 updateSystemStats() 函數
 * 收集系統資訊並更新到內部狀態結構
 */
void dms_shadow_update_system_stats(void);

/**
 * @brief 檢查設備是否已綁定到 DMS Server
 *
 * 封裝原始的 isDeviceBound() 函數
 * 檢查 Shadow 中的設備綁定資訊
 *
 * @return true 已綁定，false 未綁定
 */
bool dms_shadow_is_device_bound(void);

/**
 * @brief 獲取設備綁定資訊
 *
 * @return 設備綁定資訊結構指針
 */
const device_bind_info_t* dms_shadow_get_bind_info(void);

/**
 * @brief 獲取當前報告狀態
 *
 * @return 當前報告狀態結構指針
 */
const shadow_reported_state_t* dms_shadow_get_reported_state(void);

/**
 * @brief 檢查 Shadow Get 是否完成
 *
 * @return true 已完成，false 仍在等待或未開始
 */
bool dms_shadow_is_get_completed(void);

/**
 * @brief 清理 Shadow 模組
 *
 * 清理資源並重設狀態
 */
void dms_shadow_cleanup(void);

/*-----------------------------------------------------------*/

#endif /* DMS_SHADOW_H_ */

