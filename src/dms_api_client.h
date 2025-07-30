
/*
 * DMS API Client Header
 * HTTP Client specifically designed for DMS Server API integration
 * Supports HMAC-SHA1 signature and DMS API protocol
 */

#ifndef DMS_API_CLIENT_H_
#define DMS_API_CLIENT_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "demo_config.h"
/*-----------------------------------------------------------*/

/* DMS API 配置 */
#define DMS_API_BASE_URL_TEST         "https://dms-test.benq.com/api/"
#define DMS_API_PRODUCT_KEY           "DMS_Client_LINUX_APP_wvUVTQouuAMjriK5Vr7dO8ZIUkWOZ5wa"
#define DMS_API_PRODUCT_TYPE          "instashow"

/* HTTP 請求配置 */
#define DMS_HTTP_TIMEOUT_MS           5000
#define DMS_HTTP_MAX_RETRIES          3
#define DMS_HTTP_USER_AGENT           "DMS-Client/1.1.0"

/* API 端點路徑 */
#define DMS_API_CONTROL_CONFIG_LIST   "v2/device/control-config/list"
#define DMS_API_CONTROL_PROGRESS      "v1/device/control/progress/update"
#define DMS_API_LOG_UPLOAD_URL        "v1/device/log/uploadurl/attain"
#define DMS_API_FW_UPDATE_LIST        "v1/device/fw-update/list"
#define DMS_API_FW_PROGRESS           "v1/device/fw/progress/update"
#define DMS_API_DEVICE_INFO_UPDATE    "v1/device/info/update"

/* 回應緩衝區大小 */
#define DMS_API_MAX_RESPONSE_SIZE     4096
#define DMS_API_MAX_URL_SIZE          1024
#define DMS_API_MAX_PAYLOAD_SIZE      4096

/*-----------------------------------------------------------*/

/**
 * @brief DMS API 回應結果
 */
typedef enum {
    DMS_API_SUCCESS = 0,
    DMS_API_ERROR_NETWORK,
    DMS_API_ERROR_HTTP,
    DMS_API_ERROR_AUTH,
    DMS_API_ERROR_TIMEOUT,
    DMS_API_ERROR_INVALID_PARAM,
    DMS_API_ERROR_JSON_PARSE,
    DMS_API_ERROR_SERVER,
    DMS_API_ERROR_MEMORY_ALLOCATION,    
    DMS_API_ERROR_DECRYPT_FAILED,
    DMS_API_ERROR_UNKNOWN
} DMSAPIResult_t;

/**
 * @brief HTTP 方法
 */
typedef enum {
    DMS_HTTP_GET = 0,
    DMS_HTTP_POST
} DMSHTTPMethod_t;

/**
 * @brief DMS API 回應結構
 */
typedef struct {
    DMSAPIResult_t result;
    long httpCode;
    char* data;
    size_t dataSize;
    char errorMessage[256];
} DMSAPIResponse_t;

/**
 * @brief 控制配置項目結構
 */
typedef struct {
    int statusProgressId;
    char item[64];
    int type;  // 1-String, 2-JSON Object
    char value[256];
} DMSControlConfig_t;

/**
 * @brief 控制結果結構
 */
typedef struct {
    int statusProgressId;
    int status;  // 1-successful, 2-failed
    char failedCode[32];
    char failedReason[128];
} DMSControlResult_t;

/**
 * @brief 日誌上傳請求結構
 */
typedef struct {
    char macAddress[32];
    char contentType[64];
    char logFile[128];
    char size[32];
    char md5[64];
} DMSLogUploadRequest_t;



/**
 * @brief 設備註冊請求結構 (根據 DMS API 文檔)
 */
typedef struct {
    char bdid[128];               // Base64 編碼的設備識別符
    char uniqueId[64];            // 設備唯一 ID (Client ID)
    char macAddress[32];          // MAC 地址 (含冒號格式)
    char serial[64];              // 設備序號
    char modelName[64];           // 型號名稱
    char panel[16];               // 面板區域 (WW/CN/etc)
    char brand[32];               // 品牌名稱
    char version[32];             // DMS Client 版本
    char type[8];                 // 設備類型�(0-5)
    int subType;                  // 設備子類型 (1-4)
    char countryCode[8];          // 國家代碼
    char architecture[256];       // 系統架構資訊
} DMSDeviceRegisterRequest_t;

/**
 * @brief PIN 碼回應結構
 */
typedef struct {
    char pincode[16];             // 6 碼 PIN Code
    uint32_t expiredAt;           // PIN Code 到期時間 (Unix timestamp)
} DMSPincodeResponse_t;

/**
 * @brief 國家代碼回應結構
 */
typedef struct {
    char countryCode[8];          // 國家代碼 (如: tw, us, cn)
} DMSCountryCodeResponse_t;

/*-----------------------------------------------------------*/


/**
 * @brief 初始化 DMS API 客戶端
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t dms_api_client_init(void);

/**
 * @brief 清理 DMS API 客戶端
 */
void dms_api_client_cleanup(void);

/**
 * @brief 生成 HMAC-SHA1 簽名
 * @param[in] message 要簽名的訊息
 * @param[in] key 簽名金鑰
 * @param[out] signature 輸出簽名 (Base64 編碼)
 * @param[in] signatureSize 簽名緩衝區大小
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t dms_generate_hmac_sha1_signature(const char* message,
                                               const char* key,
                                               char* signature,
                                               size_t signatureSize);

/**
 * @brief 執行 HTTP 請求
 * @param[in] method HTTP 方法
 * @param[in] url 完整 URL
 * @param[in] payload 請求內容 (POST 時使用)
 * @param[out] response 回應結構
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t dms_http_request(DMSHTTPMethod_t method,
                               const char* url,
                               const char* payload,
                               DMSAPIResponse_t* response);

/*-----------------------------------------------------------*/
/* DMS API 具體實作函數 */

/**
 * @brief 取得控制配置列表
 * @param[in] uniqueId 設備唯一 ID
 * @param[out] configs 控制配置陣列
 * @param[in] maxConfigs 最大配置數量
 * @param[out] configCount 實際配置數量
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t dms_api_control_config_list(const char* uniqueId,
                                          DMSControlConfig_t* configs,
                                          int maxConfigs,
                                          int* configCount);

/**
 * @brief 更新控制進度
 * @param[in] uniqueId 設備唯一 ID
 * @param[in] results 控制結果陣列
 * @param[in] resultCount 結果數量
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t dms_api_control_progress_update(const char* uniqueId,
                                              const DMSControlResult_t* results,
                                              int resultCount);

/**
 * @brief 取得日誌上傳 URL
 * @param[in] request 日誌上傳請求
 * @param[out] uploadUrl 上傳 URL (呼叫者需要提供足夠大的緩衝區)
 * @param[in] urlSize URL 緩衝區大小
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t dms_api_log_upload_url_attain(const DMSLogUploadRequest_t* request,
                                            char* uploadUrl,
                                            size_t urlSize);

/**
 * @brief 取得韌體更新列表
 * @param[in] uniqueId 設備唯一 ID
 * @param[out] response API 回應 (呼叫者需要解析 JSON)
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t dms_api_fw_update_list(const char* uniqueId, DMSAPIResponse_t* response);

/**
 * @brief 更新韌體進度
 * @param[in] macAddress 設備 MAC 地址
 * @param[in] fwProgressId 韌體進度 ID
 * @param[in] version 韌體版本
 * @param[in] status 狀態 (0:下載中, 1:成功, 2:失敗)
 * @param[in] percentage 進度百分比
 * @param[in] failedCode 失敗代碼 (可選)
 * @param[in] failedReason 失敗原因 (可選)
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t dms_api_fw_progress_update(const char* macAddress,
                                        const char* fwProgressId,
                                        const char* version,
                                        int status,
                                        int percentage,
                                        const char* failedCode,
                                        const char* failedReason);

/**
 * @brief 更新設備資訊
 * @param[in] uniqueId 設備唯一 ID
 * @param[in] versionCode 版本代碼
 * @param[in] serial 序號
 * @param[in] currentDatetime 當前時間
 * @param[in] fwVersion 韌體版本 (可選)
 * @param[in] panel 面板 (可選)
 * @param[in] countryCode 國家代碼 (可選)
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t dms_api_device_info_update(const char* uniqueId,
                                         int versionCode,
                                         const char* serial,
                                         const char* currentDatetime,
                                         const char* fwVersion,
                                         const char* panel,
                                         const char* countryCode);

/*-----------------------------------------------------------*/

/**
 * @brief 釋放 API 回應記憶體
 * @param[in] response 要釋放的回應結構
 */
void dms_api_response_free(DMSAPIResponse_t* response);

/**
 * @brief 取得 API 錯誤描述
 * @param[in] result 錯誤碼
 * @return 錯誤描述字串
 */
const char* dms_api_get_error_string(DMSAPIResult_t result);

/**
 * @brief 設定 API 基礎 URL
 * @param[in] baseUrl 基礎 URL
 */
void dms_api_set_base_url(const char* baseUrl);

/**
 * @brief 取得當前 API 基礎 URL
 * @return 當前基礎 URL
 */
const char* dms_api_get_base_url(void);


/*-----------------------------------------------------------*/


/**
 * @brief 取得 DMS Server URL 配置
 * @param[in] site 站點 ("AWS" 或 "AWS_CN")
 * @param[in] environment 環境 ("T", "S", "P")
 * @param[in] uniqueId 設備唯一 ID
 * @param[out] config 解析後的配置結構
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t dms_api_server_url_get(const char* site,
                                     const char* environment,
                                     const char* uniqueId,
                                     DMSServerConfig_t* config);


/*-----------------------------------------------------------*/
/* Crypto functions for DMS server response decryption */

/**
 * @brief Base64 解碼使用 OpenSSL
 * @param[in] input Base64 編碼的字串
 * @param[out] output 解碼後的資料 (呼叫者需要釋放)
 * @param[out] output_length 解碼後的資料大小
 * @return 成功返回 DMS_CRYPTO_SUCCESS，失敗返回錯誤碼
 */
DMSCryptoResult_t base64_decode_openssl(const char* input, 
                                       unsigned char** output, 
                                       size_t* output_length);

/**
 * @brief AES-128-CBC 解密
 * @param[in] encrypted_data 加密資料
 * @param[in] encrypted_length 加密資料大小
 * @param[in] key AES 金鑰 (16 bytes)
 * @param[in] iv 初始向量 (16 bytes)
 * @param[out] decrypted_data 解密後的資料 (呼叫者需要釋放)
 * @param[out] decrypted_length 解密後的資料大小
 * @return 成功返回 DMS_CRYPTO_SUCCESS，失敗返回錯誤碼
 */
DMSCryptoResult_t aes_128_cbc_decrypt(const unsigned char* encrypted_data,
                                     size_t encrypted_length,
                                     const unsigned char* key,
                                     const unsigned char* iv,
                                     unsigned char** decrypted_data,
                                     size_t* decrypted_length);

/**
 * @brief 解密 DMS Server 回應
 * @param[in] encrypted_base64 Base64 編碼的加密資料
 * @param[out] decrypted_json 解密後的 JSON 字串 (呼叫者需要釋放)
 * @param[out] decrypted_length 解密後的資料大小
 * @return 成功返回 DMS_CRYPTO_SUCCESS，失敗返回錯誤碼
 */
DMSCryptoResult_t decrypt_dms_server_response(const char* encrypted_base64,
                                             char** decrypted_json,
                                             size_t* decrypted_length);



/*-----------------------------------------------------------*/
/* DMS 設備註冊相關 API 函數 */

/**
 * @brief 註冊設備到 DMS Server
 * @param[in] request 設備註冊請求結構
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t dms_api_device_register(const DMSDeviceRegisterRequest_t* request);

/**
 * @brief 取得設備 PIN 碼
 * @param[in] uniqueId 設備唯一 ID
 * @param[in] deviceType 設備類型字串5)
 * @param[out] response PIN 碼回應結構
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t dms_api_device_pincode_get(const char* uniqueId, 
                                          const char* deviceType,
                                          DMSPincodeResponse_t* response);

/**
 * @brief 取得設備所在國家代碼
 * @param[in] uniqueId 設備唯一 ID
 * @param[out] response 國家代碼回應結構
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t dms_api_device_country_code_get(const char* uniqueId,
                                               DMSCountryCodeResponse_t* response);

/**
 * @brief 智慧計算 BDID (根據 unique_id 格式自動判斷)
 * @param[in] uniqueId 設備唯一 ID
 * @param[in] macAddress 實際 MAC 地址 (備用)
 * @param[out] bdid 輸出 BDID 字串
 * @param[in] bdidSize BDID 緩衝區大小
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t dms_api_calculate_smart_bdid(const char* uniqueId,
                                            const char* macAddress,
                                            char* bdid,
                                            size_t bdidSize);

/**
 * @brief Base64 編碼字串 (工具函數)
 * @param[in] input 輸入字串
 * @param[out] output 輸出 Base64 字串
 * @param[in] outputSize 輸出緩衝區大小
 * @return 成功返回 DMS_API_SUCCESS，失敗返回錯誤碼
 */
DMSAPIResult_t base64_encode_string(const char* input, char* output, size_t outputSize);



/*-----------------------------------------------------------*/


#endif /* DMS_API_CLIENT_H_ */
