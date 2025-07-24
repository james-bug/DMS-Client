
/*
 * DMS Client 簡化統一日誌系統（方案 B）
 * 只處理 DMS Client 自己的日誌，不影響 AWS IoT SDK
 *
 * 特點：
 * 1. 如果有 BCML，使用 BCML 日誌系統
 * 2. 如果沒有 BCML，使用簡單的獨立實現
 * 3. 完全不影響 AWS IoT SDK 的 LogInfo/LogError 等
 */

#ifndef DMS_LOG_H_
#define DMS_LOG_H_

#include <stdio.h>
#include <stdarg.h>

/**************************************************/
/******* 日誌級別定義 ******/
/**************************************************/

typedef enum {
    DMS_LOG_LEVEL_ERROR = 0,
    DMS_LOG_LEVEL_WARN  = 1,
    DMS_LOG_LEVEL_INFO  = 2,
    DMS_LOG_LEVEL_DEBUG = 3
} DmsLogLevel_t;

/**************************************************/
/******* BCML 整合（如果可用）******/
/**************************************************/

#ifdef BCML_MIDDLEWARE_ENABLED
/* 如果啟用 BCML，使用 BCML 的日誌系統作為後端 */
#include "bcml_log.h"

/* 日誌控制函數 */
#define dms_log_set_level(level)  bcml_set_log_level(level)
#define dms_log_get_level()       get_log_level()

/* DMS 專用的日誌宏 - 使用 BCML 後端，但加上 DMS 標識 */
#define DMS_LOG_ERROR(fmt, ...)   bcml_printf(LOG_LEVEL_ERROR, "[DMS-ERROR] " fmt "\n", ##__VA_ARGS__)
#define DMS_LOG_WARN(fmt, ...)    bcml_printf(LOG_LEVEL_WARN,  "[DMS-WARN ] " fmt "\n", ##__VA_ARGS__)
#define DMS_LOG_INFO(fmt, ...)    bcml_printf(LOG_LEVEL_INFO,  "[DMS-INFO ] " fmt "\n", ##__VA_ARGS__)
#define DMS_LOG_DEBUG(fmt, ...)   bcml_printf(LOG_LEVEL_DEBUG, "[DMS-DEBUG] " fmt "\n", ##__VA_ARGS__)

/* 模組化日誌宏 - 使用 BCML 後端 */
#define DMS_LOG_SHADOW(fmt, ...)  bcml_printf(LOG_LEVEL_INFO,  "[DMS-SHADOW] " fmt "\n", ##__VA_ARGS__)
#define DMS_LOG_API(fmt, ...)     bcml_printf(LOG_LEVEL_INFO,  "[DMS-API] " fmt "\n", ##__VA_ARGS__)
#define DMS_LOG_MQTT(fmt, ...)    bcml_printf(LOG_LEVEL_DEBUG, "[DMS-MQTT] " fmt "\n", ##__VA_ARGS__)
#define DMS_LOG_TLS(fmt, ...)     bcml_printf(LOG_LEVEL_DEBUG, "[DMS-TLS] " fmt "\n", ##__VA_ARGS__)
#define DMS_LOG_CRYPTO(fmt, ...)  bcml_printf(LOG_LEVEL_DEBUG, "[DMS-CRYPTO] " fmt "\n", ##__VA_ARGS__)

#else
/**************************************************/
/******* 獨立實現（當 BCML 未啟用時）******/
/**************************************************/

/* 全域變數宣告 */
extern DmsLogLevel_t g_dms_log_level;

/* 日誌控制函數 */
void dms_log_set_level(DmsLogLevel_t level);
DmsLogLevel_t dms_log_get_level(void);
void dms_log_printf(DmsLogLevel_t level, const char* fmt, ...);

/* DMS 專用的日誌宏 - 使用獨立實現 */
#define DMS_LOG_ERROR(fmt, ...)   dms_log_printf(DMS_LOG_LEVEL_ERROR, "[DMS-ERROR] " fmt "\n", ##__VA_ARGS__)
#define DMS_LOG_WARN(fmt, ...)    dms_log_printf(DMS_LOG_LEVEL_WARN,  "[DMS-WARN ] " fmt "\n", ##__VA_ARGS__)
#define DMS_LOG_INFO(fmt, ...)    dms_log_printf(DMS_LOG_LEVEL_INFO,  "[DMS-INFO ] " fmt "\n", ##__VA_ARGS__)
#define DMS_LOG_DEBUG(fmt, ...)   dms_log_printf(DMS_LOG_LEVEL_DEBUG, "[DMS-DEBUG] " fmt "\n", ##__VA_ARGS__)

/* 模組化日誌宏 - 使用獨立實現 */
#define DMS_LOG_SHADOW(fmt, ...)  dms_log_printf(DMS_LOG_LEVEL_INFO,  "[DMS-SHADOW] " fmt "\n", ##__VA_ARGS__)
#define DMS_LOG_API(fmt, ...)     dms_log_printf(DMS_LOG_LEVEL_INFO,  "[DMS-API] " fmt "\n", ##__VA_ARGS__)
#define DMS_LOG_MQTT(fmt, ...)    dms_log_printf(DMS_LOG_LEVEL_DEBUG, "[DMS-MQTT] " fmt "\n", ##__VA_ARGS__)
#define DMS_LOG_TLS(fmt, ...)     dms_log_printf(DMS_LOG_LEVEL_DEBUG, "[DMS-TLS] " fmt "\n", ##__VA_ARGS__)
#define DMS_LOG_CRYPTO(fmt, ...)  dms_log_printf(DMS_LOG_LEVEL_DEBUG, "[DMS-CRYPTO] " fmt "\n", ##__VA_ARGS__)

#endif /* BCML_MIDDLEWARE_ENABLED */

/**************************************************/
/******* 初始化與工具函數 ******/
/**************************************************/

/**
 * @brief 初始化 DMS 日誌系統
 * @param level 預設日誌級別
 * @return 0 成功，其他失敗
 */
int dms_log_init(DmsLogLevel_t level);

/**
 * @brief 清理 DMS 日誌系統
 */
void dms_log_cleanup(void);

/**
 * @brief 從字串解析日誌級別
 * @param level_str 級別字串 ("ERROR", "WARN", "INFO", "DEBUG")
 * @return 對應的日誌級別
 */
DmsLogLevel_t dms_log_parse_level(const char* level_str);

/**
 * @brief 取得日誌級別字串
 * @param level 日誌級別
 * @return 級別字串
 */
const char* dms_log_level_string(DmsLogLevel_t level);

/**************************************************/
/******* 初始化宏（方便在 main 中使用）******/
/**************************************************/

#define DMS_LOG_SYSTEM_INIT() dms_log_init(DMS_LOG_LEVEL_INFO)
#define DMS_LOG_SYSTEM_CLEANUP() dms_log_cleanup()

#endif /* DMS_LOG_H_ */
