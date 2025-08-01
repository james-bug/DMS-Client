
/*
 * DMS Client for OpenWrt with AWS IoT Device Shadow and DMS API Integration
 * Enhanced version with Shadow support, auto-reconnect, and HTTP Client
 * 
 * 修訂版本：移除重複全域變數，使用模組化介面
 * 保持所有原有功能和業務邏輯完全不變
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/sysinfo.h>
#include <ctype.h>

/* AWS IoT Device SDK includes */
#include "core_mqtt.h"
#include "core_json.h"
#include "transport_interface.h"
#include "clock.h"

/* OpenSSL transport implementation */
#ifdef USE_OPENSSL
#include "openssl_posix.h"
#include <openssl/ssl.h>
#endif

/* Demo configuration */
#include "demo_config.h"

/* DMS Modules */
#include "dms_aws_iot.h" 
#include "dms_shadow.h"  
#include "dms_command.h"
#include "dms_reconnect.h"
#include "dms_config.h"

/* DMS API Client */
#ifdef DMS_API_ENABLED
#include "dms_api_client.h"
#endif

/* Add Middleware support*/
#ifdef BCML_MIDDLEWARE_ENABLED
#include "bcml_adapter.h"
#endif

/* OpenSSL for MD5 calculation */
#ifdef USE_OPENSSL
#include <openssl/md5.h>
#endif

/* log system */
#include "dms_log.h"

/*-----------------------------------------------------------*/
/* 函數宣告 */

static int getRealMacAddress(char* macAddress, size_t bufferSize);
static int getDeviceHardwareInfo(DeviceHardwareInfo_t* hwInfo);
static int registerDeviceWithDMS(void);
static int checkAndRegisterDevice(void);
static int runManualRegistration(void);
static int showDeviceStatus(void);
static void formatMacForDMS(const char* input, char* output, size_t outputSize);
static void signalHandler(int signal);
static void runMainLoopWithNewModule(void);
static int loadDeviceInfoFromUCI(DeviceHardwareInfo_t* hwInfo);
static int loadDeviceInfoFromSystem(DeviceHardwareInfo_t* hwInfo);
static int parseDeviceBindInfo(const char* payload, size_t payloadLength, DeviceBindInfo_t* bindInfo);
static bool isDeviceBound(const DeviceBindInfo_t* bindInfo);

#ifdef BCML_MIDDLEWARE_ENABLED
static void show_bcml_status(void);
static int test_bcml_wifi_controls(void);
#endif

#ifdef DMS_API_ENABLED
static int executeControlConfig(const DMSControlConfig_t* config);
static int createTestLogFile(const char* filePath);
static int uploadLogFileToS3(const char* uploadUrl, const char* filePath);
static long getFileSize(const char* filePath);
static int calculateFileMD5(const char* filePath, char* md5Hash, size_t hashSize);
#endif

static int min(int a, int b) { return (a < b) ? a : b; }

/*-----------------------------------------------------------*/
/* 
 * ✅ 保留的全域變數（有正當理由）
 */

/* 程式生命週期控制 - 訊號處理必需 */
static volatile bool g_exitFlag = false;

/* 設備狀態管理 - 尚未模組化，保持現有結構 */
static DeviceHardwareInfo_t g_deviceHardwareInfo = { 0 };
static DeviceRegisterStatus_t g_deviceRegisterStatus = DEVICE_REGISTER_STATUS_UNKNOWN;
static DeviceBindInfo_t g_deviceBindInfo = { 0 };


/*-----------------------------------------------------------*/

/**
 * @brief 信號處理函數
 */
static void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        printf("\n🛑 Received termination signal (%d). Shutting down gracefully...\n", signal);
        g_exitFlag = true;
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief 檢查設備是否已綁定
 */
static bool isDeviceBound(const DeviceBindInfo_t* bindInfo)
{
    if (bindInfo == NULL) {
        return false;
    }

    return (bindInfo->bindStatus == DEVICE_BIND_STATUS_BOUND &&
            bindInfo->hasBindInfo == true);
}

/*-----------------------------------------------------------*/

/**
 * @brief 從 UCI 配置讀取設備資訊
 */
static int loadDeviceInfoFromUCI(DeviceHardwareInfo_t* hwInfo)
{
    if (hwInfo == NULL) {
        return DMS_ERROR_INVALID_PARAMETER;
    }

    printf("📋 Attempting to load device info from UCI configuration...\n");

    /* TODO: 實作 UCI 讀取邏輯 */
    /* 這裡需要使用 libuci 或 system calls 來讀取 UCI 配置 */
    /* 暫時返回失敗，讓系統繼續嘗試其他方法 */
    
    printf("⚠️  UCI configuration reading not implemented yet\n");
    return DMS_ERROR_UCI_CONFIG_FAILED;
}

/*-----------------------------------------------------------*/

/**
 * @brief 從系統檔案讀取設備資訊
 */
static int loadDeviceInfoFromSystem(DeviceHardwareInfo_t* hwInfo)
{
    FILE* fp = NULL;
    char buffer[256];
    bool foundAnyInfo = false;

    if (hwInfo == NULL) {
        return DMS_ERROR_INVALID_PARAMETER;
    }

    printf("📋 Attempting to load device info from system files...\n");

    /* 嘗試讀取設備型號 */
    fp = fopen(SYSTEM_MODEL_FILE, "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            /* 移除換行符 */
            buffer[strcspn(buffer, "\r\n")] = '\0';
            strncpy(hwInfo->modelName, buffer, sizeof(hwInfo->modelName) - 1);
            hwInfo->modelName[sizeof(hwInfo->modelName) - 1] = '\0';
            printf("   Found model from system: %s\n", hwInfo->modelName);
            foundAnyInfo = true;
        }
        fclose(fp);
    }

    /* 嘗試讀取設備序號 */
    fp = fopen(SYSTEM_SERIAL_FILE, "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            /* 移除換行符 */
            buffer[strcspn(buffer, "\r\n")] = '\0';
            strncpy(hwInfo->serialNumber, buffer, sizeof(hwInfo->serialNumber) - 1);
            hwInfo->serialNumber[sizeof(hwInfo->serialNumber) - 1] = '\0';
            printf("   Found serial from system: %s\n", hwInfo->serialNumber);
            foundAnyInfo = true;
        }
        fclose(fp);
    }

    /* 嘗試從 /proc/cpuinfo 讀取架構資訊 */
    fp = fopen(SYSTEM_CPUINFO_FILE, "r");
    if (fp != NULL) {
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            if (strstr(buffer, "model name") != NULL || strstr(buffer, "Architecture") != NULL) {
                /* 移除換行符 */
                buffer[strcspn(buffer, "\r\n")] = '\0';
                strncpy(hwInfo->architecture, buffer, sizeof(hwInfo->architecture) - 1);
                hwInfo->architecture[sizeof(hwInfo->architecture) - 1] = '\0';
                printf("   Found architecture info: %.100s...\n", hwInfo->architecture);
                foundAnyInfo = true;
                break;
            }
        }
        fclose(fp);
    }

    if (foundAnyInfo) {
        hwInfo->infoSource = DEVICE_INFO_SOURCE_SYSTEM;
        printf("✅ Successfully loaded device info from system files\n");
        return DMS_SUCCESS;
    } else {
        printf("⚠️  No device info found in system files\n");
        return DMS_ERROR_SYSTEM_FILE_ACCESS;
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief 獲取真實 MAC 地址
 */
static int getRealMacAddress(char* macAddress, size_t bufferSize)
{
    FILE* fp;
    char buffer[256];
    char interface[64];
    char mac[32];
    int foundMac = 0;

    if (macAddress == NULL || bufferSize < 18) {
        return DMS_ERROR_INVALID_PARAMETER;
    }

    /* 嘗試從 /proc/net/arp 讀取 */
    fp = fopen("/proc/net/arp", "r");
    if (fp != NULL) {
        /* 跳過標題行 */
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                if (sscanf(buffer, "%*s %*s %*s %17s %*s %63s", mac, interface) == 2) {
                    if (strcmp(mac, "00:00:00:00:00:00") != 0) {
                        strncpy(macAddress, mac, bufferSize - 1);
                        macAddress[bufferSize - 1] = '\0';
                        foundMac = 1;
                        break;
                    }
                }
            }
        }
        fclose(fp);
    }

    /* 如果沒找到，嘗試從網路介面讀取 */
    if (!foundMac) {
        fp = popen("ip link show | grep -o -E '([[:xdigit:]]{1,2}:){5}[[:xdigit:]]{1,2}' | head -1", "r");
        if (fp != NULL) {
            if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                /* 移除換行符 */
                buffer[strcspn(buffer, "\r\n")] = '\0';
                if (strlen(buffer) == 17) { /* MAC 地址長度 */
                    strncpy(macAddress, buffer, bufferSize - 1);
                    macAddress[bufferSize - 1] = '\0';
                    foundMac = 1;
                }
            }
            pclose(fp);
        }
    }

    if (foundMac) {
        printf("📡 Found MAC address: %s\n", macAddress);
        return DMS_SUCCESS;
    } else {
        printf("⚠️  Could not find MAC address, using default\n");
        strncpy(macAddress, "AB:A1:AE:69:2A:AE", bufferSize - 1);
        macAddress[bufferSize - 1] = '\0';
        return DMS_SUCCESS; /* 使用預設值仍視為成功 */
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief 獲取設備硬體資訊 (主要函數)
 */
static int getDeviceHardwareInfo(DeviceHardwareInfo_t* hwInfo) 
{
    if (hwInfo == NULL) {
        DMS_LOG_ERROR("Invalid parameter for device hardware info");
        return DMS_ERROR_INVALID_PARAMETER;
    }
    
    memset(hwInfo, 0, sizeof(DeviceHardwareInfo_t));
    hwInfo->lastUpdated = (uint32_t)time(NULL);
    
    DMS_LOG_INFO("Gathering device hardware information...");
    
    /* 優先順序: UCI -> 系統檔案 -> 硬體檢測 -> 預設值 */
    
    /* 1. 嘗試從 UCI 配置讀取 */
    if (loadDeviceInfoFromUCI(hwInfo) == DMS_SUCCESS) {
        DMS_LOG_INFO("Device info loaded from UCI configuration");
        hwInfo->isValid = true;
        return DMS_SUCCESS;
    }
    
    /* 2. 嘗試從系統檔案讀取 */
    if (loadDeviceInfoFromSystem(hwInfo) == DMS_SUCCESS) {
        DMS_LOG_INFO("Device info partially loaded from system files");
        /* 系統檔案可能只有部分資訊，繼續補充預設值 */
    } else {
        DMS_LOG_DEBUG("Using hardware detection + defaults");
    }
    
    /* 3. 補充預設值和硬體檢測 */
    
    /* 如果沒有型號，使用預設值 */
    if (strlen(hwInfo->modelName) == 0) {
        strcpy(hwInfo->modelName, DEFAULT_DEVICE_MODEL);
        DMS_LOG_DEBUG("Using default model: %s", DEFAULT_DEVICE_MODEL);
    }
    
    /* 如果沒有序號，使用預設值 */
    if (strlen(hwInfo->serialNumber) == 0) {
        strcpy(hwInfo->serialNumber, DEFAULT_DEVICE_SERIAL);
        DMS_LOG_DEBUG("Using default serial: %s", DEFAULT_DEVICE_SERIAL);
    }
    
    /* 獲取真實 MAC 地址 */
    if (getRealMacAddress(hwInfo->macAddress, sizeof(hwInfo->macAddress)) != DMS_SUCCESS) {
        strcpy(hwInfo->macAddress, "AB:A1:AE:69:2A:AE"); /* 預設值 */
        DMS_LOG_WARN("Using default MAC address");
    }
    
    /* 設定其他預設值 */
    strcpy(hwInfo->panel, DEFAULT_DEVICE_PANEL);
    strcpy(hwInfo->brand, DEFAULT_DEVICE_BRAND);
    strcpy(hwInfo->countryCode, DEFAULT_COUNTRY_CODE);
    strcpy(hwInfo->firmwareVersion, "1.0.0");
    
    /* 設定設備類型 */
    hwInfo->deviceType = DMS_DEVICE_TYPE_LINUX;
    hwInfo->deviceSubType = DMS_DEVICE_SUBTYPE_EMBEDDED;
    
    hwInfo->infoSource = (hwInfo->infoSource != DEVICE_INFO_SOURCE_SYSTEM) ? 
                         DEVICE_INFO_SOURCE_DEFAULT : hwInfo->infoSource;
    hwInfo->isValid = true;

    DMS_LOG_INFO("Device hardware info summary:");
    DMS_LOG_INFO("  Model: %s, Serial: %s", hwInfo->modelName, hwInfo->serialNumber);
    DMS_LOG_INFO("  MAC: %s, Panel: %s", hwInfo->macAddress, hwInfo->panel);
    DMS_LOG_DEBUG("  Type: %d, SubType: %d", hwInfo->deviceType, hwInfo->deviceSubType);
    DMS_LOG_DEBUG("  Source: %s", 
           (hwInfo->infoSource == DEVICE_INFO_SOURCE_SYSTEM) ? "System+Defaults" : "Defaults");
    
    return DMS_SUCCESS;
}


/*-----------------------------------------------------------*/

/**
 * @brief 解析設備綁定資訊
 */

static int parseDeviceBindInfo(const char* payload, size_t payloadLength, DeviceBindInfo_t* bindInfo)
{
    JSONStatus_t jsonResult;
    char* valueStart;
    size_t valueLength;

    if (payload == NULL || bindInfo == NULL || payloadLength == 0) {
        DMS_LOG_ERROR("Invalid parameters for bind info parsing");
        return DMS_ERROR_INVALID_PARAMETER;
    }

    /* 驗證 JSON 格式 */
    jsonResult = JSON_Validate((char*)payload, payloadLength);
    if (jsonResult != JSONSuccess) {
        DMS_LOG_ERROR("Invalid JSON format in Shadow document. Error: %d", jsonResult);
        return DMS_ERROR_SHADOW_FAILURE;
    }

    DMS_LOG_SHADOW("Parsing device bind information from Shadow...");

    /* 初始化綁定資訊 */
    memset(bindInfo, 0, sizeof(DeviceBindInfo_t));
    bindInfo->bindStatus = DEVICE_BIND_STATUS_UNKNOWN;

    /* 查找 reported.info */
    jsonResult = JSON_Search((char*)payload, payloadLength,
                            JSON_QUERY_REPORTED_INFO, strlen(JSON_QUERY_REPORTED_INFO),
                            &valueStart, &valueLength);
    
    if (jsonResult != JSONSuccess) {
        DMS_LOG_WARN("No binding info found in Shadow (info section missing)");
        bindInfo->bindStatus = DEVICE_BIND_STATUS_UNBOUND;
        return DMS_SUCCESS;
    }

    /* 解析公司名稱 */
    jsonResult = JSON_Search((char*)payload, payloadLength,
                            JSON_QUERY_COMPANY_NAME, strlen(JSON_QUERY_COMPANY_NAME),
                            &valueStart, &valueLength);
    
    if (jsonResult == JSONSuccess && valueLength > 0) {
        size_t copyLength = MIN(valueLength, sizeof(bindInfo->companyName) - 1);
        /* 移除引號 */
        if (*valueStart == '"' && valueLength >= 2) {
            valueStart++;
            copyLength = MIN(valueLength - 2, sizeof(bindInfo->companyName) - 1);
        }
        strncpy(bindInfo->companyName, valueStart, copyLength);
        bindInfo->companyName[copyLength] = '\0';
        bindInfo->hasBindInfo = true;
        DMS_LOG_DEBUG("Company Name: %s", bindInfo->companyName);
    }

    /* 解析添加者 */
    jsonResult = JSON_Search((char*)payload, payloadLength,
                            JSON_QUERY_ADDED_BY, strlen(JSON_QUERY_ADDED_BY),
                            &valueStart, &valueLength);
    
    if (jsonResult == JSONSuccess && valueLength > 0) {
        size_t copyLength = MIN(valueLength, sizeof(bindInfo->addedBy) - 1);
        /* 移除引號 */
        if (*valueStart == '"' && valueLength >= 2) {
            valueStart++;
            copyLength = MIN(valueLength - 2, sizeof(bindInfo->addedBy) - 1);
        }
        strncpy(bindInfo->addedBy, valueStart, copyLength);
        bindInfo->addedBy[copyLength] = '\0';
        DMS_LOG_DEBUG("Added By: %s", bindInfo->addedBy);
    }

    /* 解析設備名稱 */
    jsonResult = JSON_Search((char*)payload, payloadLength,
                            JSON_QUERY_DEVICE_NAME, strlen(JSON_QUERY_DEVICE_NAME),
                            &valueStart, &valueLength);
    
    if (jsonResult == JSONSuccess && valueLength > 0) {
        size_t copyLength = MIN(valueLength, sizeof(bindInfo->deviceName) - 1);
        /* 移除引號 */
        if (*valueStart == '"' && valueLength >= 2) {
            valueStart++;
            copyLength = MIN(valueLength - 2, sizeof(bindInfo->deviceName) - 1);
        }
        strncpy(bindInfo->deviceName, valueStart, copyLength);
        bindInfo->deviceName[copyLength] = '\0';
        DMS_LOG_DEBUG("Device Name: %s", bindInfo->deviceName);
    }

    /* 解析公司ID */
    jsonResult = JSON_Search((char*)payload, payloadLength,
                            JSON_QUERY_COMPANY_ID, strlen(JSON_QUERY_COMPANY_ID),
                            &valueStart, &valueLength);
    
    if (jsonResult == JSONSuccess && valueLength > 0) {
        size_t copyLength = MIN(valueLength, sizeof(bindInfo->companyId) - 1);
        /* 移除引號 */
        if (*valueStart == '"' && valueLength >= 2) {
            valueStart++;
            copyLength = MIN(valueLength - 2, sizeof(bindInfo->companyId) - 1);
        }
        strncpy(bindInfo->companyId, valueStart, copyLength);
        bindInfo->companyId[copyLength] = '\0';
        DMS_LOG_DEBUG("Company ID: %s", bindInfo->companyId);
    }

    /* 設定綁定狀態 */
    if (bindInfo->hasBindInfo && 
        strlen(bindInfo->companyName) > 0 && 
        strlen(bindInfo->companyId) > 0) {
        bindInfo->bindStatus = DEVICE_BIND_STATUS_BOUND;
        bindInfo->lastUpdated = (uint32_t)time(NULL);
        DMS_LOG_SHADOW("Device bind info parsed successfully");
    } else {
        bindInfo->bindStatus = DEVICE_BIND_STATUS_UNBOUND;
        DMS_LOG_WARN("Device is not bound to any company");
    }

    return DMS_SUCCESS;
}

#ifdef DMS_API_ENABLED


/*-----------------------------------------------------------*/

/**
 * @brief 格式化 MAC 地址給 DMS API 使用
 */
static void formatMacForDMS(const char* input, char* output, size_t outputSize)
{
    if (input == NULL || output == NULL || outputSize < 13) {
        return;
    }

    char temp[32];
    strncpy(temp, input, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    /* 移除冒號並轉大寫 */
    int outputIndex = 0;
    for (int i = 0; temp[i] != '\0' && outputIndex < (int)outputSize - 1; i++) {
        if (temp[i] != ':') {
            output[outputIndex] = toupper(temp[i]);
            outputIndex++;
        }
    }
    output[outputIndex] = '\0';

    printf("📋 Formatted MAC: %s → %s\n", input, output);
}

/*-----------------------------------------------------------*/

/**
 * @brief 獲取檔案大小
 */
static long getFileSize(const char* filePath)
{
    FILE* fp = fopen(filePath, "rb");
    if (fp == NULL) {
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }

    long size = ftell(fp);
    fclose(fp);
    return size;
}

/*-----------------------------------------------------------*/

/**
 * @brief 計算檔案 MD5
 */
static int calculateFileMD5(const char* filePath, char* md5Hash, size_t hashSize)
{
#ifdef USE_OPENSSL
    FILE* fp;
    MD5_CTX md5Context;
    unsigned char hash[MD5_DIGEST_LENGTH];
    unsigned char buffer[1024];
    size_t bytesRead;

    if (filePath == NULL || md5Hash == NULL || hashSize < 33) {
        return -1;
    }

    fp = fopen(filePath, "rb");
    if (fp == NULL) {
        return -1;
    }

    MD5_Init(&md5Context);

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        MD5_Update(&md5Context, buffer, bytesRead);
    }

    fclose(fp);
    MD5_Final(hash, &md5Context);

    /* 轉換為16進制字串 */
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        snprintf(&md5Hash[i * 2], 3, "%02x", hash[i]);
    }

    return 0;
#else
    /* 如果沒有 OpenSSL，使用簡單的假 MD5 */
    strncpy(md5Hash, "d41d8cd98f00b204e9800998ecf8427e", hashSize - 1);
    md5Hash[hashSize - 1] = '\0';
    return 0;
#endif
}

/*-----------------------------------------------------------*/

/**
 * @brief 建立測試日誌檔案
 */
static int createTestLogFile(const char* filePath)
{
    FILE* fp;
    time_t currentTime;
    struct tm* timeInfo;

    if (filePath == NULL) {
        return -1;
    }

    fp = fopen(filePath, "w");
    if (fp == NULL) {
        printf("❌ Failed to create log file: %s\n", filePath);
        return -1;
    }

    time(&currentTime);
    timeInfo = localtime(&currentTime);

    fprintf(fp, "DMS Client Log File\n");
    fprintf(fp, "Generated: %s", asctime(timeInfo));
    fprintf(fp, "Client Version: %s\n", DMS_CLIENT_VERSION);
    fprintf(fp, "Device Model: %s\n", g_deviceHardwareInfo.modelName);
    fprintf(fp, "Device Serial: %s\n", g_deviceHardwareInfo.serialNumber);
fprintf(fp, "MAC Address: %s\n", g_deviceHardwareInfo.macAddress);
    fprintf(fp, "\n--- System Information ---\n");
    
    /* 添加系統資訊 */
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        fprintf(fp, "System Uptime: %ld seconds\n", info.uptime);
        fprintf(fp, "Total RAM: %lu bytes\n", info.totalram);
        fprintf(fp, "Free RAM: %lu bytes\n", info.freeram);
        fprintf(fp, "Load Average: %lu, %lu, %lu\n", 
                info.loads[0], info.loads[1], info.loads[2]);
    }

    fprintf(fp, "\n--- Connection History ---\n");
    fprintf(fp, "This is a test log file for DMS upload functionality.\n");
    fprintf(fp, "File created for testing purposes.\n");

    fclose(fp);
    printf("✅ Test log file created: %s\n", filePath);
    return 0;
}

/*-----------------------------------------------------------*/

/**
 * @brief 執行設備註冊流程
 */
static int registerDeviceWithDMS(void)
{
    DMSAPIResult_t apiResult;
    DMSDeviceRegisterRequest_t registerRequest = {0};
    DMSPincodeResponse_t pincodeResponse = {0};
    char formattedMac[32];
    
    DMS_LOG_API("Starting device registration process...");
    
    /* 確保設備硬體資訊已獲取 */
    if (!g_deviceHardwareInfo.isValid) {
        DMS_LOG_DEBUG("Getting device hardware info first...");
        if (getDeviceHardwareInfo(&g_deviceHardwareInfo) != DMS_SUCCESS) {
            DMS_LOG_ERROR("Failed to get device hardware info");
            return DMS_ERROR_DEVICE_INFO_UNAVAILABLE;
        }
    }
    
    /* 準備註冊請求 */
    DMS_LOG_DEBUG("Preparing registration request...");
    
    /* 格式化 MAC 地址：去除冒號，轉大寫 */
    formatMacForDMS(g_deviceHardwareInfo.macAddress, formattedMac, sizeof(formattedMac));
    
    /* 使用格式化的 MAC 作為 unique_id 和 mac_address */
    strcpy(registerRequest.uniqueId, formattedMac);
    strcpy(registerRequest.macAddress, formattedMac);
    
    /* 使用產品名稱，不是系統檢測名稱 */
    strcpy(registerRequest.modelName, "WDC25");
    
    /* 使用數字版本格式 */
    strcpy(registerRequest.version, "1010000");
    
    /* 其他欄位 */
    strcpy(registerRequest.serial, g_deviceHardwareInfo.serialNumber);
    strcpy(registerRequest.panel, g_deviceHardwareInfo.panel);
    strcpy(registerRequest.brand, g_deviceHardwareInfo.brand);
    strcpy(registerRequest.countryCode, g_deviceHardwareInfo.countryCode);
    
    /* 使用標準架構值 */
    strcpy(registerRequest.architecture, "arm64-v8a");
    
    /* 設備類型 */
    strcpy(registerRequest.type, "3");
    registerRequest.subType = 3;
    
    /* 計算 BDID - 使用格式化的 MAC */
    apiResult = base64_encode_string(formattedMac, registerRequest.bdid, sizeof(registerRequest.bdid));
    if (apiResult != DMS_API_SUCCESS) {
        DMS_LOG_ERROR("BDID calculation failed");
        DMS_LOG_CRYPTO("Failed to encode MAC address to Base64");
        return DMS_ERROR_BDID_CALCULATION;
    }
    
    /* 執行註冊 */
    DMS_LOG_API("Registering device with DMS Server...");
    DMS_LOG_DEBUG("Registration data: MAC=%s, Model=%s, Type=%s", 
                 formattedMac, registerRequest.modelName, registerRequest.type);
    
    apiResult = dms_api_device_register(&registerRequest);
    
    if (apiResult != DMS_API_SUCCESS) {
        DMS_LOG_ERROR("Device registration failed with error: %d", apiResult);
        return DMS_ERROR_REGISTRATION_FAILED;
    }
    
    DMS_LOG_API("Device registration successful");
    
    /* 取得 PIN 碼 */
    DMS_LOG_API("Getting pairing PIN code...");
    apiResult = dms_api_device_pincode_get(formattedMac, "3", &pincodeResponse);
    
    if (apiResult == DMS_API_SUCCESS) {
        DMS_LOG_API("PIN code obtained: %s", pincodeResponse.pincode);
        DMS_LOG_DEBUG("PIN expires at: %u", pincodeResponse.expiredAt);
    } else {
        DMS_LOG_WARN("Failed to get PIN code, error: %d", apiResult);
    }
    
    return DMS_SUCCESS;
}
/*-----------------------------------------------------------*/

/**
 * @brief 檢查設備註冊狀態並在需要時自動註冊
 */
static int checkAndRegisterDevice(void)
{
    DMS_LOG_API("Checking device registration status...");
    
    /* 檢查設備是否已綁定 */
    if (isDeviceBound(&g_deviceBindInfo)) {
        DMS_LOG_INFO("Device is already bound to DMS Server");
        DMS_LOG_DEBUG("Company: %s (ID: %s)", 
               g_deviceBindInfo.companyName, g_deviceBindInfo.companyId);
        DMS_LOG_DEBUG("Device: %s (Added by: %s)", 
               g_deviceBindInfo.deviceName, g_deviceBindInfo.addedBy);
        g_deviceRegisterStatus = DEVICE_REGISTER_STATUS_REGISTERED;
        return DMS_SUCCESS;
    }
    
    DMS_LOG_WARN("Device is not bound - registration required");
    
    /* 檢查當前註冊狀態 */
    if (g_deviceRegisterStatus == DEVICE_REGISTER_STATUS_REGISTERING) {
        DMS_LOG_WARN("Registration already in progress");
        return DMS_ERROR_REGISTRATION_FAILED;
    }
    
    if (g_deviceRegisterStatus == DEVICE_REGISTER_STATUS_FAILED) {
        DMS_LOG_WARN("Previous registration failed, retrying...");
    }
    
    /* 執行註冊流程 */
    return registerDeviceWithDMS();
}
/*-----------------------------------------------------------*/

/**
 * @brief 執行控制配置
 */
static int executeControlConfig(const DMSControlConfig_t* config)
{
    if (config == NULL) {
        printf("❌ Invalid control config\n");
        return DMS_ERROR_INVALID_PARAMETER;
    }

    printf("🎛️  Executing control config: %s = %s (type: %d)\n", 
           config->item, config->value, config->type);

#ifdef BCML_MIDDLEWARE_ENABLED
    /* 使用 BCML 執行控制 */
    int result = bcml_execute_wifi_control(config->item, config->value);
    if (result == 0) {
        printf("✅ BCML control executed successfully\n");
        return DMS_SUCCESS;
    } else {
        printf("❌ BCML control failed with error: %d\n", result);
        return DMS_ERROR_UNKNOWN;
    }
#else
    printf("⚠️  BCML middleware not available, simulating control execution\n");
    return DMS_SUCCESS;
#endif
}

#endif /* DMS_API_ENABLED */


#ifdef BCML_MIDDLEWARE_ENABLED

/*-----------------------------------------------------------*/

/**
 * @brief 顯示 BCML 狀態
 */
static void show_bcml_status(void)
{

  printf("📊 === BCML Status Check ===\n");

    // 檢查 WiFi 狀態
    char wifi_status[1024];
    int result = bcml_get_wifi_status(wifi_status, sizeof(wifi_status));
    if (result == DMS_SUCCESS) {
        printf("📡 Current WiFi Status: %.200s%s\n",
               wifi_status, (strlen(wifi_status) > 200) ? "..." : "");
    } else {
        printf("⚠️  Failed to get WiFi status\n");
    }

    // 顯示版本資訊
    const char *version = bcml_get_version();
    if (version) {
        printf("📋 BCML Version: %s\n", version);
    }

    printf("============================\n");
}

/*-----------------------------------------------------------*/

/**
 * @brief 測試 BCML WiFi 控制
 */
static int test_bcml_wifi_controls(void)
{
    DMS_LOG_INFO("=== BCML WiFi Control Test ===");
    
    int success_count = 0;
    int total_tests = 0;
    
    /* 測試 2.4GHz 頻道控制 */
    DMS_LOG_DEBUG("Testing 2.4GHz channel control...");
    total_tests++;
    if (bcml_execute_wifi_control("channel2g", "6") == DMS_SUCCESS) {
        success_count++;
        DMS_LOG_INFO("channel2g test passed");
    } else {
        DMS_LOG_ERROR("channel2g test failed");
    }
    
    usleep(500000); /* 0.5秒延遲 */
    
    /* 測試 5GHz 頻道控制 */
    DMS_LOG_DEBUG("Testing 5GHz channel control...");
    total_tests++;
    if (bcml_execute_wifi_control("channel5g", "149") == DMS_SUCCESS) {
        success_count++;
        DMS_LOG_INFO("channel5g test passed");
    } else {
        DMS_LOG_ERROR("channel5g test failed");
    }
    
    usleep(500000); /* 0.5秒延遲 */
    
    /* 測試 SSID 控制 */
    DMS_LOG_DEBUG("Testing SSID control...");
    total_tests++;
    if (bcml_execute_wifi_control("ssid", "DMS-Test-Network") == DMS_SUCCESS) {
        success_count++;
        DMS_LOG_INFO("SSID test passed");
    } else {
        DMS_LOG_ERROR("SSID test failed");
    }
    
    /* 顯示測試結果 */
    DMS_LOG_INFO("Test Results: %d/%d passed", success_count, total_tests);
    
    if (success_count == total_tests) {
        DMS_LOG_INFO("All BCML tests passed!");
        return DMS_SUCCESS;
    } else {
        DMS_LOG_WARN("Some BCML tests failed");
        return DMS_ERROR_UNKNOWN;
    }
}

#endif /* BCML_MIDDLEWARE_ENABLED */
/*-----------------------------------------------------------*/

/**
 * @brief 手動註冊模式 (命令行觸發)
 */
static int runManualRegistration(void)
{
    printf("🔧 [REGISTER] Manual registration mode activated\n");
    
    /* 初始化設備硬體資訊 */
    if (getDeviceHardwareInfo(&g_deviceHardwareInfo) != DMS_SUCCESS) {
        printf("❌ [REGISTER] Failed to get device hardware info\n");
        return EXIT_FAILURE;
    }
    
#ifdef DMS_API_ENABLED
    /* 執行註冊流程 */
    if (registerDeviceWithDMS() == DMS_SUCCESS) {
        printf("✅ [REGISTER] Manual registration completed successfully\n");
        return EXIT_SUCCESS;
    } else {
        printf("❌ [REGISTER] Manual registration failed\n");
        return EXIT_FAILURE;
    }
#else
    printf("⚠️  DMS API not enabled, cannot perform registration\n");
    return EXIT_FAILURE;
#endif
}

/*-----------------------------------------------------------*/

/**
 * @brief 顯示設備狀態 (命令行功能)
 */
static int showDeviceStatus(void)
{
    DMS_LOG_INFO("=== Device Status Report ===");
    
    /* 獲取設備硬體資訊 */
    if (getDeviceHardwareInfo(&g_deviceHardwareInfo) != DMS_SUCCESS) {
        DMS_LOG_ERROR("Failed to get device hardware info");
        return EXIT_FAILURE;
    }
    
    DMS_LOG_INFO("Hardware Information:");
    DMS_LOG_INFO("  Model: %s", g_deviceHardwareInfo.modelName);
    DMS_LOG_INFO("  Serial: %s", g_deviceHardwareInfo.serialNumber);
    DMS_LOG_INFO("  MAC: %s", g_deviceHardwareInfo.macAddress);
    DMS_LOG_DEBUG("  Type: %d (%s), SubType: %d (%s)", 
           g_deviceHardwareInfo.deviceType,
           (g_deviceHardwareInfo.deviceType == DMS_DEVICE_TYPE_LINUX) ? "Linux" : "Other",
           g_deviceHardwareInfo.deviceSubType,
           (g_deviceHardwareInfo.deviceSubType == DMS_DEVICE_SUBTYPE_EMBEDDED) ? "Embedded" : "Other");
    DMS_LOG_DEBUG("  Brand: %s, Panel: %s", 
           g_deviceHardwareInfo.brand, g_deviceHardwareInfo.panel);
    DMS_LOG_DEBUG("  Country: %s, FW: %s", 
           g_deviceHardwareInfo.countryCode, g_deviceHardwareInfo.firmwareVersion);
    
    DMS_LOG_INFO("Registration Status:");
    switch (g_deviceRegisterStatus) {
        case DEVICE_REGISTER_STATUS_UNKNOWN:
            DMS_LOG_WARN("  Status: Unknown");
            break;
        case DEVICE_REGISTER_STATUS_UNREGISTERED:
            DMS_LOG_WARN("  Status: Not Registered");
            break;
        case DEVICE_REGISTER_STATUS_REGISTERING:
            DMS_LOG_INFO("  Status: Registration in Progress");
            break;
        case DEVICE_REGISTER_STATUS_REGISTERED:
            DMS_LOG_INFO("  Status: Registered");
            break;
        case DEVICE_REGISTER_STATUS_FAILED:
            DMS_LOG_ERROR("  Status: Registration Failed");
            break;
    }
    
    DMS_LOG_INFO("Binding Status:");
    if (isDeviceBound(&g_deviceBindInfo)) {
        DMS_LOG_INFO("  Status: Bound to DMS Server");
        DMS_LOG_INFO("  Company: %s (ID: %s)", 
               g_deviceBindInfo.companyName, g_deviceBindInfo.companyId);
        DMS_LOG_INFO("  Device Name: %s", g_deviceBindInfo.deviceName);
        DMS_LOG_INFO("  Added By: %s", g_deviceBindInfo.addedBy);
    } else {
        DMS_LOG_WARN("  Status: Not Bound");
        DMS_LOG_WARN("  Action Required: Device registration and binding needed");
    }

#ifdef BCML_MIDDLEWARE_ENABLED
    show_bcml_status();
#endif
    
    return EXIT_SUCCESS;
}

/*-----------------------------------------------------------*/

/**
 * @brief 🔧 修訂後的主迴圈函數 - 使用模組化介面
 */
static void runMainLoopWithNewModule(void)
{
    uint32_t loopCount = 0;
    uint32_t lastHeartbeatTime = 0;
    const uint32_t HEARTBEAT_INTERVAL = 60; // 60 秒心跳間隔

    DMS_LOG_INFO("Main loop started with modular architecture...");
    DMS_LOG_DEBUG("Press Ctrl+C to exit gracefully");

    while (!g_exitFlag) {
        loopCount++;
        
        /* 🔧 修訂：使用 dms_reconnect 模組檢查連接狀態 */
        if (dms_reconnect_get_state() == CONNECTION_STATE_CONNECTED) {
            
            /* 🔧 修訂：使用 dms_aws_iot 模組處理 MQTT 事件 */
            dms_result_t processResult = dms_aws_iot_process_loop(1000);
            
            if (processResult != DMS_SUCCESS) {
                DMS_LOG_ERROR("MQTT process loop failed with status: %d", processResult);
                
                /* 根據錯誤類型決定是否重連 */
                if (processResult == DMS_ERROR_NETWORK_FAILURE) {
                    DMS_LOG_WARN("Connection lost detected, initiating reconnection...");
                    /* 🔧 修訂：使用 dms_reconnect 模組更新狀態 */
                    dms_reconnect_update_failure();
                } else {
                    DMS_LOG_ERROR("Unrecoverable MQTT error detected, exiting...");
                    break;
                }
            }

            /* 🔧 修訂：使用模組化的心跳和狀態更新 */
            uint32_t currentTime = (uint32_t)time(NULL);
            if (currentTime - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
                
                /* 建立狀態結構並更新 */
                shadow_reported_state_t current_state = {0};
                strcpy(current_state.deviceId, CLIENT_IDENTIFIER);
                strcpy(current_state.status, "online");
                current_state.connected = true;
                current_state.uptime = currentTime;
                current_state.lastHeartbeat = currentTime;
                
                /* 🔧 修訂：使用 dms_shadow 模組更新狀態 */
                if (dms_shadow_update_reported(&current_state) == DMS_SUCCESS) {
                    DMS_LOG_DEBUG("Heartbeat sent successfully (loop: %u)", loopCount);
                    lastHeartbeatTime = currentTime;
                } else {
                    DMS_LOG_WARN("Failed to send heartbeat (loop: %u)", loopCount);
                }
            }

        } else if (dms_reconnect_get_state() == CONNECTION_STATE_DISCONNECTED) {
            
            /* 🔧 修訂：使用 dms_reconnect 模組處理重連 */
            if (dms_reconnect_should_retry()) {
                uint32_t delay = dms_reconnect_get_next_delay();
                DMS_LOG_INFO("Attempting reconnection (delay: %u seconds)...", delay);
                
                if (delay > 0) {
                    DMS_LOG_DEBUG("Waiting %u seconds before reconnect attempt...", delay);
                    for (uint32_t i = 0; i < delay && !g_exitFlag; i++) {
                        sleep(1);
                        if (i % 10 == 0 && i > 0) {
                            DMS_LOG_DEBUG("%u seconds remaining...", delay - i);
                        }
                    }
                }
                
                if (!g_exitFlag) {
                    /* 🔧 修訂：使用 dms_reconnect 模組執行重連 */
                    dms_result_t reconnectResult = dms_reconnect_attempt();
                    if (reconnectResult == DMS_SUCCESS) {
                        DMS_LOG_INFO("Reconnection successful!");
                        lastHeartbeatTime = 0; // 重設心跳時間，立即發送一次
                    } else {
                        DMS_LOG_WARN("Reconnection failed, will retry...");
                    }
                }
            } else {
                DMS_LOG_ERROR("Maximum reconnection attempts reached, exiting...");
                break;
            }
            
        } else {
            /* 連接中或其他狀態，短暫等待 */
            DMS_LOG_DEBUG("Connection state: %d, waiting...", dms_reconnect_get_state());
            usleep(500000); /* 500ms */
        }

        /* 短暫休眠，避免 CPU 過度使用 */
        if (dms_reconnect_get_state() == CONNECTION_STATE_CONNECTED) {
            usleep(100000); /* 100ms - 連接時較短的間隔 */
        } else {
            usleep(1000000); /* 1秒 - 未連接時較長的間隔 */
        }

        /* 每1000次循環顯示一次狀態 */
        if (loopCount % 1000 == 0) {
            DMS_LOG_DEBUG("Main loop running: %u iterations completed", loopCount);
        }
    }

    DMS_LOG_INFO("Main loop ended (total loops: %u)", loopCount);
}

/*-----------------------------------------------------------*/
/**
 * @brief 主函數 - 改進版本，採用正確的 dms_log 級別分類
 */
int main(int argc, char **argv)
{
    int returnStatus = EXIT_SUCCESS;

    /* 信號處理設定 - 保持原有邏輯 */
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    /* 系統啟動資訊 - 使用正確的日誌級別 */
    DMS_LOG_SYSTEM_INIT();
    printf("\n🚀 === DMS Client Starting ===\n");
    DMS_LOG_INFO("DMS Client Version: %s", DMS_CLIENT_VERSION);
    DMS_LOG_INFO("Build Date: %s %s", __DATE__, __TIME__);
#ifdef DMS_API_ENABLED
    DMS_LOG_INFO("DMS API: Enabled");
#else
    DMS_LOG_INFO("DMS API: Disabled");
#endif
    DMS_LOG_INFO("Features: Shadow Support, Auto-Reconnect, DMS API Integration");

    /* === 🔧 修訂：模組化初始化序列 === */
    DMS_LOG_INFO("=== Step 1: Module Initialization ===");
    
    /* 1. 配置管理初始化 */
    if (dms_config_init() != DMS_SUCCESS) {
        DMS_LOG_ERROR("Configuration initialization failed");
        return EXIT_FAILURE;
    }
    DMS_LOG_INFO("Configuration initialized successfully");

    /* 2. AWS IoT 模組初始化 */
    const dms_config_t* config = dms_config_get();
    if (dms_aws_iot_init(config) != DMS_SUCCESS) {
        DMS_LOG_ERROR("AWS IoT module initialization failed");
        return EXIT_FAILURE;
    }
    DMS_LOG_INFO("AWS IoT module initialized successfully");

    /* 3. Shadow 模組初始化 */
    mqtt_interface_t mqtt_if = dms_aws_iot_get_interface();
    if (dms_shadow_init(&mqtt_if) != DMS_SUCCESS) {
        DMS_LOG_ERROR("Shadow module initialization failed");
        return EXIT_FAILURE;
    }
    DMS_LOG_SHADOW("Shadow module initialized successfully");

    /* 4. 命令處理模組初始化 */
    if (dms_command_init() != DMS_SUCCESS) {
        DMS_LOG_ERROR("Command module initialization failed");
        return EXIT_FAILURE;
    }
    DMS_LOG_INFO("Command module initialized successfully");

    /* 5. 重連模組初始化 */
    const dms_reconnect_config_t* reconnect_config = dms_config_get_reconnect();
    if (dms_reconnect_init(reconnect_config) != DMS_SUCCESS) {
        DMS_LOG_ERROR("Reconnect module initialization failed");
        return EXIT_FAILURE;
    }

    /* 6. 註冊重連介面 */
    dms_reconnect_interface_t reconnect_interface = {
        .connect = dms_aws_iot_connect,
        .disconnect = dms_aws_iot_disconnect,
        .restart_shadow = dms_shadow_start
    };
    dms_reconnect_register_interface(&reconnect_interface);
    DMS_LOG_DEBUG("Interface: connect=%p, disconnect=%p, restart_shadow=%p",
                 (void*)reconnect_interface.connect,
                 (void*)reconnect_interface.disconnect, 
                 (void*)reconnect_interface.restart_shadow);
    DMS_LOG_INFO("Reconnect module initialized successfully");

#ifdef BCML_MIDDLEWARE_ENABLED
    /* 7. BCML 中間件初始化 */
    if (bcml_adapter_init() == DMS_SUCCESS) {
        dms_command_register_bcml_handler(bcml_execute_wifi_control);
        DMS_LOG_INFO("BCML adapter initialized and registered");
    } else {
        DMS_LOG_WARN("BCML adapter initialization failed");
    }
#endif

#ifdef DMS_API_ENABLED
    /* 8. DMS API 客戶端初始化 */
    if (dms_api_client_init() == DMS_API_SUCCESS) {
        DMS_LOG_API("DMS API client initialized successfully");
    } else {
        DMS_LOG_WARN("DMS API client initialization failed");
    }
#endif

    /* === 解析命令列參數 - 使用正確的日誌級別 === */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("\n📖 === Usage Information ===\n");
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --help, -h          Show this help message\n");
            printf("  --test, -t          Run connection test\n");
            printf("  --registration, -r  Run manual device registration\n");
            printf("  --status, -s        Show device status\n");
            printf("  --debug, -d         Enable debug logging\n");
            printf("  --log-level <level> Set log level (ERROR/WARN/INFO/DEBUG)\n");
            printf("  --version, -v       Show version information\n");
#ifdef BCML_MIDDLEWARE_ENABLED
            printf("  --bcml-test         Test BCML WiFi controls\n");
#endif
            return EXIT_SUCCESS;
        }
        else if (strcmp(argv[i], "--test") == 0 || strcmp(argv[i], "-t") == 0) {
            DMS_LOG_INFO("=== Running Connection Test ===");
            
            /* 🔧 修訂：使用模組化介面進行測試 */
            DMS_LOG_DEBUG("Testing modular AWS IoT connection...");
            if (dms_aws_iot_connect() == DMS_SUCCESS) {
                DMS_LOG_INFO("AWS IoT connection test successful");
                
                /* 測試 Shadow 功能 */
                DMS_LOG_SHADOW("Testing Shadow functionality...");
                if (dms_shadow_get_document() == DMS_SUCCESS) {
                    DMS_LOG_SHADOW("Shadow test successful");
                } else {
                    DMS_LOG_WARN("Shadow test failed");
                }
                
                dms_aws_iot_disconnect();
            } else {
                DMS_LOG_ERROR("AWS IoT connection test failed");
            }
            
            return EXIT_SUCCESS;
        }
        else if (strcmp(argv[i], "--registration") == 0 || strcmp(argv[i], "-r") == 0) {
            return runManualRegistration();
        }
        else if (strcmp(argv[i], "--status") == 0 || strcmp(argv[i], "-s") == 0) {
            return showDeviceStatus();
        }
        else if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            DMS_LOG_INFO("Debug logging enabled");
            dms_log_set_level(DMS_LOG_LEVEL_DEBUG);
        }
        else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
            const char* level_str = argv[++i];
            DmsLogLevel_t level = dms_log_parse_level(level_str);
            dms_log_set_level(level);
            DMS_LOG_INFO("Log level set to: %s", dms_log_level_string(level));
        }
        else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("\n📋 === Version Information ===\n");
            DMS_LOG_INFO("DMS Client Version: %s", DMS_CLIENT_VERSION);
            DMS_LOG_INFO("Build Date: %s %s", __DATE__, __TIME__);
            DMS_LOG_INFO("AWS IoT SDK: Embedded C SDK");
            DMS_LOG_INFO("TLS Library: OpenSSL");
#ifdef BCML_MIDDLEWARE_ENABLED
            DMS_LOG_INFO("BCML Middleware: Enabled");
#else
            DMS_LOG_INFO("BCML Middleware: Disabled");
#endif
#ifdef DMS_API_ENABLED
            DMS_LOG_INFO("DMS API: Enabled");
#else
            DMS_LOG_INFO("DMS API: Disabled");
#endif
            return EXIT_SUCCESS;
        }
#ifdef BCML_MIDDLEWARE_ENABLED
        else if (strcmp(argv[i], "--bcml-test") == 0) {
            DMS_LOG_INFO("=== BCML Test Mode ===");
            return test_bcml_wifi_controls();
        }
#endif
        /* 其他參數處理保持原有邏輯... */
    }

    /* === 第二階段：建立連接 - 🔧 修訂：使用模組化介面和正確日誌級別 === */
    DMS_LOG_INFO("=== Step 2: Establishing Connection ===");
    
    /* 🔧 修訂：使用 dms_aws_iot 模組建立連接 */
    if (dms_aws_iot_connect() != DMS_SUCCESS) {
        DMS_LOG_ERROR("Failed to establish AWS IoT connection");
        DMS_LOG_WARN("Will attempt reconnection in main loop...");
        /* 不立即退出，讓重連模組處理 */
    } else {
        DMS_LOG_INFO("AWS IoT connection established successfully");
        
        /* 🔧 修訂：使用 dms_reconnect 模組重設狀態 */
        dms_reconnect_reset_state();
        
        /* 🔧 修訂：使用 dms_shadow 模組訂閱主題 */
        if (dms_shadow_subscribe_topics() == DMS_SUCCESS) {
            DMS_LOG_SHADOW("Shadow topics subscribed successfully");
        } else {
            DMS_LOG_WARN("Shadow subscription failed, will retry in main loop");
        }
    }

    /* === 第三階段：設備註冊和綁定檢查 === */
    DMS_LOG_INFO("=== Step 3: Device Registration Check ===");
    
    /* 獲取設備硬體資訊 */
    if (getDeviceHardwareInfo(&g_deviceHardwareInfo) != DMS_SUCCESS) {
        DMS_LOG_WARN("Failed to get complete device hardware info, using defaults");
    } else {
        DMS_LOG_DEBUG("Device hardware info loaded successfully");
    }

#ifdef DMS_API_ENABLED
    /* 🔧 修訂：使用 dms_shadow 模組獲取 Shadow 文件 */
    DMS_LOG_SHADOW("Checking device binding status from Shadow...");
    if (dms_shadow_get_document() == DMS_SUCCESS) {
        /* Shadow 回調將處理綁定資訊解析 */
        sleep(2); /* 等待 Shadow 回應 */
        
        if (isDeviceBound(&g_deviceBindInfo)) {
            DMS_LOG_INFO("Device is bound to DMS Server");
            g_deviceRegisterStatus = DEVICE_REGISTER_STATUS_REGISTERED;
        } else {
            DMS_LOG_WARN("Device is not bound, checking registration...");
            if (checkAndRegisterDevice() == DMS_SUCCESS) {
                DMS_LOG_API("Device registration completed");
            } else {
                DMS_LOG_WARN("Device registration failed, will continue without DMS features");
            }
        }
    } else {
        DMS_LOG_WARN("Failed to get Shadow document, will retry in main loop");
    }
#endif

    /* === 第四階段：主要運行循環 === */
    DMS_LOG_INFO("=== Step 4: Starting Main Loop ===");
    
    /* 🔧 修訂：使用新的模組化主迴圈 */
    runMainLoopWithNewModule();

    /* === 清理資源 - 🔧 修訂：使用模組化清理和正確日誌級別 === */
cleanup:
    DMS_LOG_INFO("=== DMS Client Shutdown ===");
    
    /* 模組化清理順序（與初始化相反） */
#ifdef DMS_API_ENABLED
    dms_api_client_cleanup();
    DMS_LOG_API("DMS API client cleaned up");
#endif

#ifdef BCML_MIDDLEWARE_ENABLED
    bcml_adapter_cleanup();
    DMS_LOG_INFO("BCML adapter cleaned up");
#endif

    dms_command_cleanup();
    DMS_LOG_INFO("Command module cleaned up");
    
    dms_reconnect_cleanup();
    DMS_LOG_INFO("Reconnect module cleaned up");
    
    dms_shadow_cleanup();
    DMS_LOG_SHADOW("Shadow module cleaned up");
    
    dms_aws_iot_disconnect();
    dms_aws_iot_cleanup();
    DMS_LOG_INFO("AWS IoT module cleaned up");
    
    dms_config_cleanup();
    DMS_LOG_INFO("Configuration cleaned up");
    
    DMS_LOG_SYSTEM_CLEANUP();
    
    return returnStatus;
}


