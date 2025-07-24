

/*
 * DMS Client Configuration Management Module
 * Centralized configuration for AWS IoT, DMS API, and reconnection settings
 */

#ifndef DMS_CONFIG_H_
#define DMS_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


/* 使用 demo_config.h 中已定義的錯誤碼 */
#include "demo_config.h"

/* dms_result_t 就是 demo_config.h 中的 DMSErrorCode_t */
typedef DMSErrorCode_t dms_result_t;


/*-----------------------------------------------------------*/
/* 配置結構體定義 */

/**
 * @brief AWS IoT 連接配置
 */
typedef struct {
    char aws_endpoint[256];              // AWS IoT 端點
    char client_id[64];                  // MQTT 客戶端 ID
    char ca_cert_path[256];              // 根憑證路徑
    char client_cert_path[256];          // 客戶端憑證路徑
    char private_key_path[256];          // 私鑰路徑
    uint16_t mqtt_port;                  // MQTT 端口
    uint16_t keep_alive_seconds;         // Keep Alive 間隔
    uint32_t connack_recv_timeout_ms;    // CONNACK 接收超時
    uint32_t process_loop_timeout_ms;    // 處理循環超時
    uint32_t network_buffer_size;        // 網路緩衝區大小
    uint32_t transport_timeout_ms;       // 傳輸超時
} dms_aws_iot_config_t;

/**
 * @brief DMS API 配置
 */
typedef struct {
    char base_url[256];                  // API 基礎 URL
    char product_key[128];               // 產品密鑰
    char product_type[32];               // 產品類型
    char user_agent[64];                 // User Agent
    uint32_t timeout_ms;                 // HTTP 超時
    uint8_t max_retries;                 // 最大重試次數
} dms_api_config_t;

/**
 * @brief 重連策略配置
 */
typedef struct {
    uint8_t max_retry_attempts;          // 最大重試次數
    uint16_t base_delay_seconds;         // 基礎延遲秒數
    uint16_t max_delay_seconds;          // 最大延遲秒數
    uint32_t shadow_get_timeout_ms;      // Shadow Get 超時
    bool enable_exponential_backoff;     // 啟用指數退避
} dms_reconnect_config_t;

/**
 * @brief 完整配置結構
 */
typedef struct {
    dms_aws_iot_config_t aws_iot;        // AWS IoT 配置
    dms_api_config_t api;                // DMS API 配置
    dms_reconnect_config_t reconnect;    // 重連配置
    bool initialized;                    // 初始化標記
} dms_config_t;

/*-----------------------------------------------------------*/
/* 公開介面函數 */

/**
 * @brief 初始化配置管理模組
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_config_init(void);

/**
 * @brief 獲取完整配置
 * @return 配置結構指針，如果未初始化則返回 NULL
 */
const dms_config_t* dms_config_get(void);

/**
 * @brief 獲取 AWS IoT 配置
 * @return AWS IoT 配置指針，如果未初始化則返回 NULL
 */
const dms_aws_iot_config_t* dms_config_get_aws_iot(void);

/**
 * @brief 獲取 DMS API 配置
 * @return DMS API 配置指針，如果未初始化則返回 NULL
 */
const dms_api_config_t* dms_config_get_api(void);

/**
 * @brief 獲取重連配置
 * @return 重連配置指針，如果未初始化則返回 NULL
 */
const dms_reconnect_config_t* dms_config_get_reconnect(void);

/**
 * @brief 驗證配置有效性
 * @return DMS_SUCCESS 配置有效，其他為錯誤碼
 */
dms_result_t dms_config_validate(void);

/**
 * @brief 清理配置管理模組
 */
void dms_config_cleanup(void);

/*-----------------------------------------------------------*/
/* 便利宏定義 (向後相容) */

#define DMS_CONFIG_AWS_ENDPOINT()        (dms_config_get_aws_iot()->aws_endpoint)
#define DMS_CONFIG_CLIENT_ID()           (dms_config_get_aws_iot()->client_id)
#define DMS_CONFIG_MQTT_PORT()           (dms_config_get_aws_iot()->mqtt_port)
#define DMS_CONFIG_CA_CERT_PATH()        (dms_config_get_aws_iot()->ca_cert_path)
#define DMS_CONFIG_CLIENT_CERT_PATH()    (dms_config_get_aws_iot()->client_cert_path)
#define DMS_CONFIG_PRIVATE_KEY_PATH()    (dms_config_get_aws_iot()->private_key_path)

#define DMS_CONFIG_API_BASE_URL()        (dms_config_get_api()->base_url)
#define DMS_CONFIG_PRODUCT_KEY()         (dms_config_get_api()->product_key)
#define DMS_CONFIG_PRODUCT_TYPE()        (dms_config_get_api()->product_type)

#define DMS_CONFIG_MAX_RETRIES()         (dms_config_get_reconnect()->max_retry_attempts)
#define DMS_CONFIG_BASE_DELAY()          (dms_config_get_reconnect()->base_delay_seconds)

#endif /* DMS_CONFIG_H_ */
