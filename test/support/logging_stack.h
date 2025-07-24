
// test/support/logging_stack.h
#ifndef LOGGING_STACK_H_
#define LOGGING_STACK_H_

/* 空的日誌宏定義，避免編譯錯誤 */
#define LogError(message)
#define LogWarn(message)
#define LogInfo(message)
#define LogDebug(message)

/* 如果需要實際的日誌輸出，可以重定向到 printf */
// #define LogError(message)   printf("ERROR: " message "\n")
// #define LogWarn(message)    printf("WARN: " message "\n")
// #define LogInfo(message)    printf("INFO: " message "\n")
// #define LogDebug(message)   printf("DEBUG: " message "\n")

#endif /* LOGGING_STACK_H_ */
