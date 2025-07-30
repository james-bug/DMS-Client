
/*
 * Demo configuration for DMS Client with AWS IoT Device Shadow
 * Updated to use AWS IoT Device Shadow topics and JSON format
 * Added DMS Server API integration support
 */

#ifndef DEMO_CONFIG_H_
#define DEMO_CONFIG_H_

/**************************************************/
/******* DO NOT CHANGE the following order ******/
/**************************************************/

/* Include logging macros */
#include "logging_levels.h"

/* Logging configuration for the Demo. */
#ifndef LIBRARY_LOG_NAME
    #define LIBRARY_LOG_NAME    "DMS_CLIENT"
#endif

#ifndef LIBRARY_LOG_LEVEL
    #define LIBRARY_LOG_LEVEL    LOG_INFO
#endif

#include "logging_stack.h"

/************ End of logging configuration ****************/

/* AWS IoT 端點設定 */
#define AWS_IOT_ENDPOINT                "apexd90h2t5wg-ats.iot.eu-central-1.amazonaws.com"

/* MQTT 連接埠 */
#define AWS_MQTT_PORT                   ( 8883 )

/* 客戶端識別符 */
#define CLIENT_IDENTIFIER               "benq-dms-test-ABA1AE692AAE"

/* 憑證路徑 */
#define ROOT_CA_CERT_PATH              "/etc/dms-client/rootCA.pem"
#define CLIENT_CERT_PATH               "/etc/dms-client/dms_pem.crt"
#define CLIENT_PRIVATE_KEY_PATH        "/etc/dms-client/dms_private.pem.key"

/* MQTT 配置 */
#define MQTT_KEEP_ALIVE_INTERVAL_SECONDS    ( 60 )
#define CONNACK_RECV_TIMEOUT_MS            ( 1000 )
#define MQTT_PROCESS_LOOP_TIMEOUT_MS       ( 1000 )

/* 網路緩衝區大小 */
#define NETWORK_BUFFER_SIZE                ( 2048 )

/* 傳輸逾時 */
#define TRANSPORT_SEND_RECV_TIMEOUT_MS     ( 5000 )

/* AWS IoT Device Shadow 主題配置 */
#define SHADOW_UPDATE_TOPIC                "$aws/things/" CLIENT_IDENTIFIER "/shadow/update"
#define SHADOW_UPDATE_ACCEPTED_TOPIC       "$aws/things/" CLIENT_IDENTIFIER "/shadow/update/accepted"
#define SHADOW_UPDATE_REJECTED_TOPIC       "$aws/things/" CLIENT_IDENTIFIER "/shadow/update/rejected"
#define SHADOW_UPDATE_DELTA_TOPIC          "$aws/things/" CLIENT_IDENTIFIER "/shadow/update/delta"
#define SHADOW_GET_TOPIC                   "$aws/things/" CLIENT_IDENTIFIER "/shadow/get"
#define SHADOW_GET_ACCEPTED_TOPIC          "$aws/things/" CLIENT_IDENTIFIER "/shadow/get/accepted"
#define SHADOW_GET_REJECTED_TOPIC          "$aws/things/" CLIENT_IDENTIFIER "/shadow/get/rejected"

/* 舊版主題配置 (向後相容) */
#define PUBLISH_TOPIC                      "dms/device/status"
#define MAX_SUBSCRIBE_TOPICS               5

/* 重連配置 */
#define MAX_RETRY_ATTEMPTS                ( 10 )
#define RETRY_BACKOFF_BASE_SECONDS        ( 2 )
#define RETRY_BACKOFF_MAX_SECONDS         ( 300 )
#define CONNECTION_RETRY_DELAY_MS         ( 1000 )


/* 新增：基於 MAC 地址的隨機退避配置 */
#define MAC_SEED_MULTIPLIER                ( 1 )    /* MAC seed 權重 */
#define MAC_SEED_MAX_OFFSET                ( 10 )   /* 最大隨機偏移（秒）*/


/* AWS IoT SDK 版本 */
#define AWS_IOT_SDK_VERSION               "202412.00"

/* DMS Client 特定配置 */
#define DMS_CLIENT_VERSION                "1.1.0"
#define MAX_MESSAGE_SIZE                  ( 512 )
#define MAX_TOPIC_LENGTH                  ( 128 )

/* DMS Server API 配置 */
#define DMS_API_ENABLED                   1
#define DMS_API_BASE_URL_TEST             "https://dms-test.benq.com/api/"
#define DMS_API_PRODUCT_KEY               "DMS_Client_LINUX_APP_wvUVTQouuAMjriK5Vr7dO8ZIUkWOZ5wa"
#define DMS_API_PRODUCT_TYPE              "instashow"

/* HTTP 請求配置 */
#define DMS_HTTP_TIMEOUT_MS               5000
#define DMS_HTTP_MAX_RETRIES              3
#define DMS_HTTP_USER_AGENT               "DMS-Client/1.1.0"

/* 設備資訊 */
#define DEVICE_TYPE                       "OpenWrt-DMS-Device"
#define FIRMWARE_VERSION                  "1.1.0"

/* 日誌配置 */
#ifndef LOG_LEVEL
    #define LOG_LEVEL                     LOG_INFO
#endif

/* 調試配置 */
#ifdef DEBUG
    #define DMS_DEBUG_PRINT(fmt, ...)    printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
    #define DMS_DEBUG_PRINT(fmt, ...)    do {} while(0)
#endif

/* 錯誤碼定義 */
typedef enum {
    DMS_SUCCESS = 0,
    DMS_ERROR_INVALID_PARAMETER,
    DMS_ERROR_NETWORK_FAILURE,
    DMS_ERROR_MQTT_FAILURE,
    DMS_ERROR_TLS_FAILURE,
    DMS_ERROR_MEMORY_ALLOCATION,
    DMS_ERROR_FILE_NOT_FOUND,
    DMS_ERROR_TIMEOUT,
    DMS_ERROR_RECONNECT_FAILED,
    DMS_ERROR_SHADOW_FAILURE,
    DMS_ERROR_DEVICE_INFO_UNAVAILABLE,     // 無法獲取設備資訊
    DMS_ERROR_UCI_CONFIG_FAILED,           // UCI 配置讀取失敗
    DMS_ERROR_SYSTEM_FILE_ACCESS,          // 系統檔案存取失敗
    DMS_ERROR_REGISTRATION_FAILED,         // 設備註冊失敗
    DMS_ERROR_PINCODE_FAILED,              // PIN 碼獲取失敗
    DMS_ERROR_BDID_CALCULATION,            // BDID 計算失敗
    DMS_ERROR_UNKNOWN
} DMSErrorCode_t;

/* 連線狀態定義 */
typedef enum {
    CONNECTION_STATE_DISCONNECTED = 0,
    CONNECTION_STATE_CONNECTING,
    CONNECTION_STATE_CONNECTED,
    CONNECTION_STATE_RECONNECTING,
    CONNECTION_STATE_ERROR
} ConnectionState_t;

/* 設備狀態定義 */
typedef enum {
    DEVICE_STATUS_OFFLINE = 0,
    DEVICE_STATUS_ONLINE,
    DEVICE_STATUS_CONNECTING,
    DEVICE_STATUS_ERROR,
    DEVICE_STATUS_MAINTENANCE
} DeviceStatus_t;

/* 訊息類型定義 */
typedef enum {
    MSG_TYPE_SHADOW_UPDATE = 0,
    MSG_TYPE_SHADOW_GET,
    MSG_TYPE_STATUS,
    MSG_TYPE_COMMAND,
    MSG_TYPE_RESPONSE,
    MSG_TYPE_TELEMETRY,
    MSG_TYPE_ALERT
} MessageType_t;

/* DMS 命令類型定義 */
typedef enum {
    DMS_CMD_NONE = 0,
    DMS_CMD_CONTROL_CONFIG_CHANGE,
    DMS_CMD_UPLOAD_LOGS,
    DMS_CMD_FW_UPGRADE,
    DMS_CMD_UNKNOWN
} DMSCommandType_t;

/* DMS 命令結構 */
typedef struct {
    DMSCommandType_t type;
    int value;
    char key[64];
    uint32_t timestamp;
    bool processed;
} DMSCommand_t;

/* DMS 命令處理結果 */
typedef enum {
    DMS_CMD_RESULT_SUCCESS = 0,
    DMS_CMD_RESULT_FAILED,
    DMS_CMD_RESULT_PENDING,
    DMS_CMD_RESULT_UNKNOWN
} DMSCommandResult_t;

/* 設備綁定狀態 */
typedef enum {
    DEVICE_BIND_STATUS_UNKNOWN = 0,
    DEVICE_BIND_STATUS_UNBOUND,
    DEVICE_BIND_STATUS_BOUND,
    DEVICE_BIND_STATUS_ERROR
} DeviceBindStatus_t;

/* 設備綁定資訊結構 */
typedef struct {
    DeviceBindStatus_t bindStatus;
    char companyName[64];
    char addedBy[64];
    char deviceName[64];
    char companyId[32];
    uint32_t lastUpdated;
    bool hasBindInfo;
} DeviceBindInfo_t;

/* Shadow 狀態結構 */
typedef struct {
    char deviceId[64];
    char deviceType[32];
    char firmwareVersion[16];
    DeviceStatus_t status;
    uint32_t uptime;
    uint32_t lastHeartbeat;
    bool connected;
    float cpuUsage;
    float memoryUsage;
    uint64_t networkBytesSent;
    uint64_t networkBytesReceived;
} ShadowReportedState_t;

/* 重連狀態結構 */
typedef struct {
    ConnectionState_t state;
    uint32_t retryCount;
    uint32_t nextRetryDelaySeconds;
    uint32_t lastConnectTime;
    uint32_t totalReconnects;
    char macAddressSeed[18];        /* 新增：MAC 地址作為 seed */
    uint32_t seedValue;             /* 新增：計算出的數字 seed */
} ReconnectState_t;

/* 設備資訊結構 */
typedef struct {
    char deviceId[64];
    char deviceType[32];
    char firmwareVersion[16];
    DeviceStatus_t status;
    uint32_t uptime;
    uint32_t lastHeartbeat;
} DeviceInfo_t;

/* 訊息結構 */
typedef struct {
    MessageType_t type;
    uint32_t timestamp;
    char topic[MAX_TOPIC_LENGTH];
    char payload[MAX_MESSAGE_SIZE];
    uint16_t payloadLength;
} DMSMessage_t;



/* AES 解密錯誤碼 */
typedef enum {
    DMS_CRYPTO_SUCCESS = 0,
    DMS_CRYPTO_ERROR_INVALID_PARAM,
    DMS_CRYPTO_ERROR_BASE64_DECODE,
    DMS_CRYPTO_ERROR_AES_DECRYPT,
    DMS_CRYPTO_ERROR_MEMORY_ALLOCATION,
    DMS_CRYPTO_ERROR_OPENSSL_INIT
} DMSCryptoResult_t;


/* DMS Server 配置結構 */
typedef struct {
    char apiUrl[256];
    char mqttUrl[256];
    char mqttIotUrl[256];
    char mdaJsonUrl[256];
    bool hasCertInfo;
    char certPath[256];
    char certMd5[64];
    int certSize;
} DMSServerConfig_t;



/* DMS 設備類型定義 (根據 DMS Server 規範) */
typedef enum {
    DMS_DEVICE_TYPE_PUBLIC_DISPLAY = 0,    // Public Display
    DMS_DEVICE_TYPE_IFP = 1,               // Interactive Flat Panel  
    DMS_DEVICE_TYPE_SIGNAGE = 2,           // Signage
    DMS_DEVICE_TYPE_PROJECTOR = 3,         // Projector (當前設備類型)
    DMS_DEVICE_TYPE_OPS = 4,               // Open Pluggable Specification
    DMS_DEVICE_TYPE_LINUX = 5              // Linux (新增：支援 Linux 設備類型)	    
} DMSDeviceType_t;

/* DMS 設備子類型定義 (根據 DMS Server 規範) */
typedef enum {
    DMS_DEVICE_SUBTYPE_ANDROID = 1,        // Android
    DMS_DEVICE_SUBTYPE_COMBO = 2,          // Combo
    DMS_DEVICE_SUBTYPE_EMBEDDED = 3,       // Embedded (當前設備子類型)
    DMS_DEVICE_SUBTYPE_WINDOWS = 4         // Windows
} DMSDeviceSubType_t;

/* 設備資訊來源定義 */
typedef enum {
    DEVICE_INFO_SOURCE_UCI = 0,            // 從 UCI 配置讀取
    DEVICE_INFO_SOURCE_SYSTEM,             // 從系統檔案讀取  
    DEVICE_INFO_SOURCE_HARDWARE,           // 從硬體資訊讀取
    DEVICE_INFO_SOURCE_DEFAULT             // 使用編譯時預設值
} DeviceInfoSource_t;

/* 設備硬體資訊結構 */
typedef struct {
    char modelName[32];                     // 設備型號 (如: WDC25)
    char serialNumber[32];                  // 設備序號 (如: S090Y00000002)
    char macAddress[18];                    // MAC 地址 (如: AB:A1:AE:69:2A:AE)
    char panel[8];                         // 面板區域 (如: WW, CN)
    char brand[16];                         // 品牌名稱 (如: BenQ)
    DMSDeviceType_t deviceType;             // 設備類型 (0-4)
    DMSDeviceSubType_t deviceSubType;       // 設備子類型 (1-4)
    char countryCode[4];                    // 國家代碼 (如: tw)
    char firmwareVersion[16];               // 韌體版本
    char architecture[16];                 // 系統架構資訊
    DeviceInfoSource_t infoSource;         // 資訊來源
    bool isValid;                           // 資訊是否有效
    uint32_t lastUpdated;                   // 最後更新時間
} DeviceHardwareInfo_t;

/* 設備註冊狀態 */
typedef enum {
    DEVICE_REGISTER_STATUS_UNKNOWN = 0,
    DEVICE_REGISTER_STATUS_UNREGISTERED,
    DEVICE_REGISTER_STATUS_REGISTERING,
    DEVICE_REGISTER_STATUS_REGISTERED,
    DEVICE_REGISTER_STATUS_FAILED
} DeviceRegisterStatus_t;



/* DMS AES 解密相關常數 */
#define DMS_AES_KEY_SIZE              16
#define DMS_AES_IV_SIZE               16
#define DMS_AES_BLOCK_SIZE            16

/* DMS 加密參數 - 根據 API 文檔 */
#define DMS_AES_KEY                   "NBWTF9EYV8pRHhAz"
#define DMS_AES_IV                    "TNFbj2fha4ZJVDFF"


/* 功能宏定義 */
#define ARRAY_SIZE(x)                     (sizeof(x) / sizeof((x)[0]))
#define MIN(a, b)                         ((a) < (b) ? (a) : (b))
#define MAX(a, b)                         ((a) > (b) ? (a) : (b))

/* JSON 解析相關宏 */
#define MAX_JSON_BUFFER_SIZE              ( 1024 )
#define MAX_JSON_KEY_LENGTH               ( 128 )
#define MAX_JSON_VALUE_LENGTH             ( 256 )

/* DMS 命令相關宏 */
#define DMS_COMMAND_KEY_CONTROL_CONFIG    "control-config-change"
#define DMS_COMMAND_KEY_UPLOAD_LOGS       "upload_logs"
#define DMS_COMMAND_KEY_FW_UPGRADE        "fw_upgrade"

/* JSON 查詢路徑 */
#define JSON_QUERY_DESIRED_STATE          "state.desired"
#define JSON_QUERY_CONTROL_CONFIG         "state.control-config-change"
#define JSON_QUERY_UPLOAD_LOGS           "state.upload_logs"
#define JSON_QUERY_FW_UPGRADE            "state.fw_upgrade"


/* Shadow 綁定資訊查詢路徑 */
#define JSON_QUERY_REPORTED_INFO          "state.reported.info"
#define JSON_QUERY_COMPANY_NAME           "state.reported.info.company_name"
#define JSON_QUERY_ADDED_BY               "state.reported.info.added_by"
#define JSON_QUERY_DEVICE_NAME            "state.reported.info.device_name"
#define JSON_QUERY_COMPANY_ID             "state.reported.info.company_id"

/* Shadow Get 相關 */
#define SHADOW_GET_TIMEOUT_MS             ( 5000 )
#define SHADOW_GET_MAX_RETRIES            ( 3 )

/* 字串安全操作 */
#define SAFE_STRNCPY(dest, src, size)     do { \
    strncpy(dest, src, size - 1); \
    dest[size - 1] = '\0'; \
} while(0)

/* 時間相關宏 */
#define SECONDS_TO_MS(s)                  ((s) * 1000)
#define MINUTES_TO_MS(m)                  ((m) * 60 * 1000)
#define HOURS_TO_MS(h)                    ((h) * 60 * 60 * 1000)

/* Shadow JSON 模板 */
#define SHADOW_REPORTED_JSON_TEMPLATE \
    "{"                               \
    "\"state\":{"                     \
    "\"reported\":{"                  \
    "\"connected\":%s,"               \
    "\"status\":\"%s\","              \
    "\"uptime\":%u,"                  \
    "\"timestamp\":%u,"               \
    "\"firmware\":\"%s\","            \
    "\"device_type\":\"%s\","         \
    "\"cpu_usage\":%.2f,"             \
    "\"memory_usage\":%.2f,"          \
    "\"network_sent\":%lu,"           \
    "\"network_received\":%lu"        \
    "}}}"

/* Shadow 命令重設 JSON 模板 */
#define SHADOW_RESET_COMMAND_JSON_TEMPLATE \
    "{"                                    \
    "\"state\":{"                          \
    "\"desired\":{"                        \
    "\"%s\":null"                          \
    "},"                                   \
    "\"reported\":{"                       \
    "\"%s\":0"                             \
    "}}}"

/* Shadow 命令結果回報 JSON 模板 */
#define SHADOW_COMMAND_RESULT_JSON_TEMPLATE \
    "{"                                     \
    "\"state\":{"                           \
    "\"reported\":{"                        \
    "\"%s_result\":%d,"                     \
    "\"%s_timestamp\":%u"                   \
    "}}}"

/* 指數退避計算 */
#define CALCULATE_BACKOFF_DELAY(retry_count) \
    MIN(RETRY_BACKOFF_BASE_SECONDS * (1 << (retry_count)), RETRY_BACKOFF_MAX_SECONDS)

/* 新的基於 MAC seed 的退避計算宏 */
#define CALCULATE_BACKOFF_DELAY_WITH_MAC_SEED(retry_count, reconnect_state) \
    calculateBackoffDelayWithSeed(retry_count, (reconnect_state)->macAddressSeed)


/* 設備資訊相關宏定義 */
#define MAX_DEVICE_MODEL_LENGTH           64
#define MAX_DEVICE_SERIAL_LENGTH          64
#define MAX_MAC_ADDRESS_LENGTH            32
#define MAX_PANEL_LENGTH                  16
#define MAX_BRAND_LENGTH                  32
#define MAX_COUNTRY_CODE_LENGTH           8
#define MAX_FIRMWARE_VERSION_LENGTH       32
#define MAX_ARCHITECTURE_LENGTH           256

/* Client ID 格式檢查宏 */
#define DMS_CLIENT_ID_PREFIX              "benq-dms-test-"
#define DMS_CLIENT_ID_PREFIX_LENGTH       14
#define DMS_MAC_SUFFIX_LENGTH             12

/* BDID 計算相關宏 */
#define MAX_BDID_LENGTH                   128
#define MAX_SOURCE_DATA_LENGTH            128

/* UCI 配置路徑 */
#define UCI_DMS_PACKAGE                   "dms-client"
#define UCI_DEVICE_SECTION                "hardware"
#define UCI_DEVICE_MODEL                  "model"
#define UCI_DEVICE_SERIAL                 "serial"
#define UCI_DEVICE_TYPE                   "device_type"
#define UCI_DEVICE_SUBTYPE                "device_subtype"
#define UCI_DEVICE_PANEL                  "panel"
#define UCI_DEVICE_BRAND                  "brand"
#define UCI_DEVICE_COUNTRY                "country_code"

/* 系統檔案路徑 (OpenWrt 常見位置) */
#define SYSTEM_MODEL_FILE                 "/proc/device-tree/model"
#define SYSTEM_SERIAL_FILE                "/proc/device-tree/serial-number"
#define SYSTEM_CPUINFO_FILE               "/proc/cpuinfo"


/* 設備硬體資訊 (預設值 - 可透過 UCI 或系統檔案覆蓋) */
#define DEFAULT_DEVICE_MODEL              "WDC25"
#define DEFAULT_DEVICE_SERIAL             "S090Y00000002"
#define DEFAULT_DEVICE_PANEL              "WW"
#define DEFAULT_DEVICE_BRAND              "BenQ"
#define DEFAULT_COUNTRY_CODE              "tw"



/* 新的退避計算函數宣告 */
uint32_t calculateBackoffDelayWithSeed(uint32_t retryCount, const char* macSeed);
void initializeMacAddressSeed(ReconnectState_t* reconnectState);
uint32_t calculateSeedFromMac(const char* macAddress);


#endif /* ifndef DEMO_CONFIG_H_ */
