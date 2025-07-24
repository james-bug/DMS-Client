
/*
 * DMS Client 簡化統一日誌系統實現（方案 B）
 */

#include "dms_log.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

/**************************************************/
/******* 非 BCML 環境的獨立實現 ******/
/**************************************************/

#ifndef BCML_MIDDLEWARE_ENABLED

/* 全域日誌級別 */
DmsLogLevel_t g_dms_log_level = DMS_LOG_LEVEL_INFO;

/**
 * @brief 設定日誌級別
 */
void dms_log_set_level(DmsLogLevel_t level) {
    g_dms_log_level = level;
    printf("[DMS-INFO ] Log level set to %s\n", dms_log_level_string(level));
}

/**
 * @brief 取得當前日誌級別
 */
DmsLogLevel_t dms_log_get_level(void) {
    return g_dms_log_level;
}

/**
 * @brief 日誌輸出函數（獨立實現）
 */
void dms_log_printf(DmsLogLevel_t level, const char* fmt, ...) {
    /* 級別過濾 */
    if (level > g_dms_log_level) {
        return;
    }

    /* 輸出日誌內容 */
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    /* 確保輸出 */
    fflush(stdout);
}

#endif /* !BCML_MIDDLEWARE_ENABLED */

/**************************************************/
/******* 共用工具函數 ******/
/**************************************************/

/**
 * @brief 初始化 DMS 日誌系統
 */
int dms_log_init(DmsLogLevel_t level) {
    dms_log_set_level(level);

    DMS_LOG_INFO("=== DMS Log System Initialized ===");
    DMS_LOG_INFO("Default log level: %s", dms_log_level_string(level));

#ifdef BCML_MIDDLEWARE_ENABLED
    DMS_LOG_INFO("BCML logging backend: ENABLED");
#else
    DMS_LOG_INFO("BCML logging backend: DISABLED (standalone mode)");
#endif

    return 0;
}

/**
 * @brief 清理 DMS 日誌系統
 */
void dms_log_cleanup(void) {
    DMS_LOG_INFO("DMS Log System cleanup completed");
}

/**
 * @brief 從字串解析日誌級別
 */
DmsLogLevel_t dms_log_parse_level(const char* level_str) {
    if (!level_str) {
        return DMS_LOG_LEVEL_INFO;
    }

    if (strcasecmp(level_str, "ERROR") == 0) {
        return DMS_LOG_LEVEL_ERROR;
    } else if (strcasecmp(level_str, "WARN") == 0 || strcasecmp(level_str, "WARNING") == 0) {
        return DMS_LOG_LEVEL_WARN;
    } else if (strcasecmp(level_str, "INFO") == 0) {
        return DMS_LOG_LEVEL_INFO;
    } else if (strcasecmp(level_str, "DEBUG") == 0) {
        return DMS_LOG_LEVEL_DEBUG;
    }

    return DMS_LOG_LEVEL_INFO; /* 預設值 */
}

/**
 * @brief 取得日誌級別字串
 */
const char* dms_log_level_string(DmsLogLevel_t level) {
    switch (level) {
        case DMS_LOG_LEVEL_ERROR: return "ERROR";
        case DMS_LOG_LEVEL_WARN:  return "WARN";
        case DMS_LOG_LEVEL_INFO:  return "INFO";
        case DMS_LOG_LEVEL_DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}
