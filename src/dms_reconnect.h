


/*
 * DMS Reconnect Module Header
 *
 * 重連策略模組 - 基於現有 demo_config.h 定義
 * 提取自 dms_client.c 的 attemptReconnection() 邏輯
 */

#ifndef DMS_RECONNECT_H_
#define DMS_RECONNECT_H_

#include "dms_config.h"
#include "dms_log.h"
#include "demo_config.h"        // ✅ 使用專案現有的狀態定義
#include <stdint.h>
#include <stdbool.h>

/*-----------------------------------------------------------*/
/* 重連狀態類型 - ✅ 直接使用 demo_config.h 中的定義 */

typedef ConnectionState_t dms_reconnect_state_t;

/*-----------------------------------------------------------*/
/* 重連介面定義 - 用於依賴注入 */

/**
 * @brief 連接建立函數類型
 * 對應 dms_aws_iot_connect()
 */
typedef dms_result_t (*dms_reconnect_connect_func_t)(void);

/**
 * @brief 連接斷開函數類型
 * 對應 dms_aws_iot_disconnect()
 */
typedef dms_result_t (*dms_reconnect_disconnect_func_t)(void);

/**
 * @brief Shadow 重啟函數類型
 * 對應 dms_shadow_start()
 */
typedef dms_result_t (*dms_reconnect_shadow_restart_func_t)(void);

/**
 * @brief 重連介面結構
 *
 * 實現依賴注入，避免模組間直接依賴
 */
typedef struct {
    dms_reconnect_connect_func_t connect;           // AWS IoT 連接函數
    dms_reconnect_disconnect_func_t disconnect;     // AWS IoT 斷開函數
    dms_reconnect_shadow_restart_func_t restart_shadow; // Shadow 重啟函數
} dms_reconnect_interface_t;

/*-----------------------------------------------------------*/
/* 公開介面函數 */

/**
 * @brief 初始化重連模組
 *
 * @param config 重連配置（從 dms_config_get_reconnect() 獲得）
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_reconnect_init(const dms_reconnect_config_t* config);

/**
 * @brief 註冊重連介面
 *
 * 實現依賴注入，避免直接依賴 AWS IoT 和 Shadow 模組
 *
 * @param interface 重連介面結構
 */
void dms_reconnect_register_interface(const dms_reconnect_interface_t* interface);

/**
 * @brief 執行重連嘗試
 *
 * 封裝原始的 attemptReconnection() 函數邏輯
 *
 * @return DMS_SUCCESS 成功，其他為錯誤碼
 */
dms_result_t dms_reconnect_attempt(void);

/**
 * @brief 檢查是否應該重連
 *
 * @return true 應該重連，false 已達最大重試次數
 */
bool dms_reconnect_should_retry(void);

/**
 * @brief 獲取下次重連延遲時間
 *
 * 封裝指數退避算法，包含 MAC 地址種子邏輯
 *
 * @return 延遲秒數
 */
uint32_t dms_reconnect_get_next_delay(void);

/**
 * @brief 重設重連狀態
 *
 * 重連成功後調用，重設重試計數等
 */
void dms_reconnect_reset_state(void);

/**
 * @brief 更新重連失敗狀態
 *
 * 重連失敗後調用，更新重試計數和延遲時間
 */
void dms_reconnect_update_failure(void);

/**
 * @brief 獲取當前重連狀態
 *
 * @return 當前連接狀態
 */
dms_reconnect_state_t dms_reconnect_get_state(void);

/**
 * @brief 獲取重連統計資訊
 *
 * @param retry_count 輸出當前重試次數（可以為 NULL）
 * @param total_reconnects 輸出總重連次數（可以為 NULL）
 */
void dms_reconnect_get_stats(uint32_t* retry_count, uint32_t* total_reconnects);

/**
 * @brief 清理重連模組
 */
void dms_reconnect_cleanup(void);

#endif /* DMS_RECONNECT_H_ */
