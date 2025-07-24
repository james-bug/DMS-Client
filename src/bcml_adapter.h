
#ifndef BCML_ADAPTER_H
#define BCML_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

#ifdef BCML_MIDDLEWARE_ENABLED
#include "bcml_config.h"  // 使用新的 API 介面
#include <cjson/cJSON.h>
#endif

/*-----------------------------------------------------------*/
/* 錯誤碼定義 */

#define DMS_SUCCESS                    0
#define DMS_ERROR_INVALID_PARAMETER   -1
#define DMS_ERROR_MIDDLEWARE_FAILED   -2
#define DMS_ERROR_UNSUPPORTED         -3
#define DMS_ERROR_JSON_PARSE          -4
#define DMS_ERROR_MEMORY_ALLOCATION   -5

/*-----------------------------------------------------------*/
/* WiFi 控制項目枚舉 */

typedef enum {
    WIFI_CONTROL_CHANNEL_2G = 0,     // 2.4GHz 頻道
    WIFI_CONTROL_CHANNEL_5G,         // 5GHz 頻道
    WIFI_CONTROL_POWER_2G,           // 2.4GHz 功率
    WIFI_CONTROL_POWER_5G,           // 5GHz 功率
    WIFI_CONTROL_BANDWIDTH_2G,       // 2.4GHz 頻寬
    WIFI_CONTROL_BANDWIDTH_5G,       // 5GHz 頻寬
    WIFI_CONTROL_MODE,               // WiFi 模式 (AP/STA/Mesh)
    WIFI_CONTROL_UNKNOWN             // 未知類型
} WiFiControlType_t;

/*-----------------------------------------------------------*/
/* WiFi 控制參數結構 */

typedef struct {
    WiFiControlType_t type;
    char item[32];                   // 控制項目名稱
    char value[64];                  // 控制值
    uint32_t timestamp;              // 時間戳
} WiFiControlParams_t;

/*-----------------------------------------------------------*/
/* WiFi 狀態結構 */

typedef struct {
    int channel2g;                   // 2.4GHz 頻道
    int channel5g;                   // 5GHz 頻道
    int power2g;                     // 2.4GHz 功率 (%)
    int power5g;                     // 5GHz 功率 (%)
    int bandwidth2g;                 // 2.4GHz 頻寬 (MHz)
    int bandwidth5g;                 // 5GHz 頻寬 (MHz)
    char mode[16];                   // WiFi 模式
    uint32_t lastUpdated;            // 最後更新時間
    bool isValid;                    // 狀態是否有效
} WiFiStatus_t;

/*-----------------------------------------------------------*/
/* 公開函數介面 */

/**
 * @brief 初始化 BCML 適配器
 * @return 成功返回 DMS_SUCCESS，失敗返回錯誤碼
 */
int bcml_adapter_init(void);

/**
 * @brief 清理 BCML 適配器
 */
void bcml_adapter_cleanup(void);

/**
 * @brief 執行 WiFi 控制 (支援 channel2g/channel5g)
 * @param item 控制項目名稱 (如: "channel2g", "channel5g")
 * @param value 控制值 (如: "6", "149")
 * @return 成功返回 DMS_SUCCESS，失敗返回錯誤碼
 */
int bcml_execute_wifi_control(const char *item, const char *value);

/**
 * @brief 獲取 WiFi 狀態 (JSON 格式)
 * @param status_json 輸出 JSON 字串緩衝區
 * @param json_size 緩衝區大小
 * @return 成功返回 DMS_SUCCESS，失敗返回錯誤碼
 */
int bcml_get_wifi_status(char *status_json, size_t json_size);

/**
 * @brief 獲取 WiFi 狀態 (結構化格式)
 * @param status 輸出 WiFi 狀態結構
 * @return 成功返回 DMS_SUCCESS，失敗返回錯誤碼
 */
int bcml_get_wifi_status_struct(WiFiStatus_t *status);

/**
 * @brief 解析控制項目類型
 * @param item 控制項目名稱
 * @return 控制項目類型枚舉
 */
WiFiControlType_t bcml_parse_control_type(const char *item);

/**
 * @brief 驗證控制參數
 * @param type 控制類型
 * @param value 控制值
 * @return 成功返回 DMS_SUCCESS，失敗返回錯誤碼
 */
int bcml_validate_control_params(WiFiControlType_t type, const char *value);

/*-----------------------------------------------------------*/
/* 除錯和測試函數 */

/**
 * @brief 執行 WiFi 控制測試序列
 * @return 成功返回 DMS_SUCCESS，失敗返回錯誤碼
 */
int bcml_test_wifi_controls(void);

/**
 * @brief 獲取 BCML 版本資訊
 * @return BCML 版本字串，若未啟用則返回 NULL
 */
const char *bcml_get_version(void);

#endif // BCML_ADAPTER_H
