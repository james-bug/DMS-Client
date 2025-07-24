
// test/support/bcml_log.h - 空的BCML Mock
#ifndef BCML_LOG_H_
#define BCML_LOG_H_

/* 空的BCML日誌定義，避免編譯錯誤 */
#define LOG_LEVEL_ERROR   0
#define LOG_LEVEL_WARN    1
#define LOG_LEVEL_INFO    2
#define LOG_LEVEL_DEBUG   3

/* 空的函數宏定義 */
#define bcml_printf(level, fmt, ...)  do {} while(0)
#define bcml_set_log_level(level)     do {} while(0)
#define get_log_level()               (LOG_LEVEL_INFO)

#endif /* BCML_LOG_H_ */
