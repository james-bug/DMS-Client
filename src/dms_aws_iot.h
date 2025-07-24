
/*
 * DMS AWS IoT Module
 *
 * This module encapsulates AWS IoT connectivity functionality extracted from dms_client.c
 * All functions maintain identical behavior to the original implementation.
 *
 * Functions extracted:
 * - establishTlsConnection()  → dms_aws_iot_establish_tls()
 * - establishMqttConnection() → dms_aws_iot_establish_mqtt()
 * - eventCallback()          → dms_aws_iot_event_callback()
 * - Connection state management
 */

#ifndef DMS_AWS_IOT_H_
#define DMS_AWS_IOT_H_

/*-----------------------------------------------------------*/
/* 包含必要的標頭檔 */

#include "dms_config.h"
#include "dms_log.h"

/* AWS IoT SDK Headers - 與原始程式碼完全相同 */
#include "core_mqtt.h"
#include "core_mqtt_state.h"

/* Transport layer - 與原始程式碼完全相同 */
#ifdef USE_OPENSSL
#include "openssl_posix.h"

/* 完整定義 NetworkContext 結構 - 與 dms_client.c 完全相同 */
#ifndef NETWORKCONTEXT_DEFINED
#define NETWORKCONTEXT_DEFINED
struct NetworkContext
{
    OpensslParams_t * pParams;
};
#endif /* NETWORKCONTEXT_DEFINED */
#endif

#include <stdint.h>
#include <stdbool.h>

/*-----------------------------------------------------------*/
/* 類型定義 */

/**
 * @brief MQTT 訊息回調函數類型
 * 與原始 eventCallback 完全相容
 */
typedef void (*mqtt_message_callback_t)(const char* topic,
                                       const char* payload,
                                       size_t payload_length);

/**
 * @brief MQTT 介面結構 - 為依賴注入做準備
 * 這個介面將提供給 Shadow 模組使用
 */
typedef struct {
    dms_result_t (*publish)(const char* topic, const char* payload, size_t len);
    dms_result_t (*subscribe)(const char* topic, mqtt_message_callback_t callback);
    bool (*is_connected)(void);
    dms_result_t (*process_loop)(uint32_t timeout_ms);
} mqtt_interface_t;

/**
 * @brief AWS IoT 連接狀態
 */
typedef enum {
    AWS_IOT_STATE_DISCONNECTED = 0,
    AWS_IOT_STATE_TLS_CONNECTED,
    AWS_IOT_STATE_MQTT_CONNECTED,
    AWS_IOT_STATE_ERROR
} aws_iot_connection_state_t;

/**
 * @brief AWS IoT 模組上下文
 * 封裝原始的全域變數
 */
typedef struct {
    MQTTContext_t mqtt_context;         // 原 g_mqttContext
    NetworkContext_t network_context;   // 原 g_networkContext
    MQTTFixedBuffer_t fixed_buffer;     // 原 g_fixedBuffer
    aws_iot_connection_state_t state;
    uint32_t last_process_time;
    mqtt_message_callback_t message_callback;
} aws_iot_context_t;

/*-----------------------------------------------------------*/
/* 公開介面函數 */

/**
 * @brief 初始化 AWS IoT 模組
 *
 * 這個函數會：
 * 1. 載入配置
 * 2. 初始化內部狀態
 * 3. 準備連接參數
 *
 * @param config 配置結構指針 (從 dms_config_get() 獲得)
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_aws_iot_init(const dms_config_t* config);

/**
 * @brief 建立完整的 AWS IoT 連接
 *
 * 這個函數封裝了原始的：
 * - establishTlsConnection()
 * - establishMqttConnection()
 *
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_aws_iot_connect(void);

/**
 * @brief 斷開 AWS IoT 連接
 *
 * 封裝原始的 cleanup() 函數功能
 *
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_aws_iot_disconnect(void);

/**
 * @brief 發佈 MQTT 訊息
 *
 * 封裝原始的 MQTT_Publish 呼叫
 *
 * @param topic 主題
 * @param payload 負載
 * @param payload_length 負載長度
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_aws_iot_publish(const char* topic,
                                const char* payload,
                                size_t payload_length);

/**
 * @brief 訂閱 MQTT 主題
 *
 * 封裝原始的 MQTT_Subscribe 呼叫
 *
 * @param topic 主題
 * @param callback 訊息回調函數
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_aws_iot_subscribe(const char* topic,
                                  mqtt_message_callback_t callback);

/**
 * @brief 處理 MQTT 事件循環
 *
 * 封裝原始的 MQTT_ProcessLoop 呼叫
 * 注意：AWS IoT SDK 的 MQTT_ProcessLoop 內建 timeout，通常約 1000ms
 *
 * @param timeout_ms 此參數保留用於介面相容性，實際 timeout 由 SDK 內建控制
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_aws_iot_process_loop(uint32_t timeout_ms);

/**
 * @brief 檢查連接狀態
 *
 * @return true 已連接，false 未連接
 */
bool dms_aws_iot_is_connected(void);

/**
 * @brief 獲取 MQTT 上下文
 *
 * 提供給需要直接操作 MQTT 的函數使用
 *
 * @return MQTT 上下文指針
 */
MQTTContext_t* dms_aws_iot_get_mqtt_context(void);

/**
 * @brief 獲取網路上下文
 *
 * 提供給需要直接操作網路的函數使用
 *
 * @return 網路上下文指針
 */
NetworkContext_t* dms_aws_iot_get_network_context(void);

/**
 * @brief 獲取 MQTT 介面
 *
 * 為依賴注入提供介面，給 Shadow 模組使用
 *
 * @return MQTT 介面結構
 */
mqtt_interface_t dms_aws_iot_get_interface(void);

/**
 * @brief 註冊訊息回調函數
 *
 * 允許其他模組註冊訊息處理器
 *
 * @param callback 回調函數
 */
void dms_aws_iot_register_message_callback(mqtt_message_callback_t callback);

/**
 * @brief 獲取連接狀態
 *
 * @return 當前連接狀態
 */
aws_iot_connection_state_t dms_aws_iot_get_state(void);

/**
 * @brief 清理 AWS IoT 模組
 *
 * 釋放資源並重設狀態
 */
void dms_aws_iot_cleanup(void);

/*-----------------------------------------------------------*/
/* 內部函數聲明 (從原始 static 函數提取) */

/**
 * @brief 建立 TLS 連接
 *
 * 這是原始 establishTlsConnection() 函數的直接移植
 * 邏輯完全相同，只是改為使用配置模組
 *
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_aws_iot_establish_tls(void);

/**
 * @brief 建立 MQTT 連接
 *
 * 這是原始 establishMqttConnection() 函數的直接移植
 * 邏輯完全相同，只是改為使用配置模組
 *
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_aws_iot_establish_mqtt(void);

/**
 * @brief MQTT 事件回調
 *
 * 這是原始 eventCallback() 函數的直接移植
 * 邏輯完全相同，會將訊息分發給註冊的回調函數
 *
 * @param pMqttContext MQTT 上下文
 * @param pPacketInfo 封包資訊
 * @param pDeserializedInfo 解析後的資訊
 */
void dms_aws_iot_event_callback(MQTTContext_t* pMqttContext,
                               MQTTPacketInfo_t* pPacketInfo,
                               MQTTDeserializedInfo_t* pDeserializedInfo);

#endif /* DMS_AWS_IOT_H_ */
