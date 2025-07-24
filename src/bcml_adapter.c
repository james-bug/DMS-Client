
// src/bcml_adapter.c - BCML Middleware 整合適配器
#include "bcml_adapter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef BCML_MIDDLEWARE_ENABLED

/*-----------------------------------------------------------*/
/* 內部函數宣告 */

static cJSON* convert_dms_to_bcml_wireless(const char *item, const char *value);
static int validate_channel_value(int channel, bool is5g);
static int validate_power_value(int power);
static const char *control_type_to_string(WiFiControlType_t type);

/*-----------------------------------------------------------*/
/* 實作函數 */

/**
 * @brief 初始化 BCML Middleware
 */
int bcml_adapter_init(void) {
    printf("🔧 [BCML] Initializing BCML Middleware adapter\n");

    // 測試基本 API 調用 (使用新的 bcml_config API，增大緩衝區)
    char test_buffer[1024];  // 增大緩衝區從 256 到 1024
    bool result = bcml_config_get("wireless", test_buffer, sizeof(test_buffer));
    if (result) {
        printf("✅ [BCML] Wireless module accessible\n");
        printf("📋 [BCML] Current config: %.200s%s\n",
               test_buffer, (strlen(test_buffer) > 200) ? "..." : "");
    } else {
        printf("⚠️  [BCML] Warning: Wireless module test failed\n");
    }

    printf("✅ [BCML] Adapter initialization completed\n");
    return DMS_SUCCESS;
}

/**
 * @brief 清理 BCML 適配器
 */
void bcml_adapter_cleanup(void) {
    printf("🧹 [BCML] Cleaning up BCML adapter\n");
    // BCML 不需要特別的清理操作
}

/**
 * @brief 解析控制項目類型
 */
WiFiControlType_t bcml_parse_control_type(const char *item) {
    if (!item) return WIFI_CONTROL_UNKNOWN;

    if (strcmp(item, "channel2g") == 0) {
        return WIFI_CONTROL_CHANNEL_2G;
    } else if (strcmp(item, "channel5g") == 0) {
        return WIFI_CONTROL_CHANNEL_5G;
    } else if (strcmp(item, "power2g") == 0) {
        return WIFI_CONTROL_POWER_2G;
    } else if (strcmp(item, "power5g") == 0) {
        return WIFI_CONTROL_POWER_5G;
    } else if (strcmp(item, "bandwidth2g") == 0) {
        return WIFI_CONTROL_BANDWIDTH_2G;
    } else if (strcmp(item, "bandwidth5g") == 0) {
        return WIFI_CONTROL_BANDWIDTH_5G;
    } else if (strcmp(item, "mode") == 0) {
        return WIFI_CONTROL_MODE;
    }

    return WIFI_CONTROL_UNKNOWN;
}

/**
 * @brief 驗證控制參數
 */
int bcml_validate_control_params(WiFiControlType_t type, const char *value) {
    if (!value) return DMS_ERROR_INVALID_PARAMETER;

    int int_value = atoi(value);

    switch (type) {
        case WIFI_CONTROL_CHANNEL_2G:
            return validate_channel_value(int_value, false);

        case WIFI_CONTROL_CHANNEL_5G:
            return validate_channel_value(int_value, true);

        case WIFI_CONTROL_POWER_2G:
        case WIFI_CONTROL_POWER_5G:
            return validate_power_value(int_value);

        case WIFI_CONTROL_BANDWIDTH_2G:
            // 2.4GHz 支援 20/40MHz
            if (int_value == 20 || int_value == 40) {
                return DMS_SUCCESS;
            }
            break;

        case WIFI_CONTROL_BANDWIDTH_5G:
            // 5GHz 支援 20/40/80/160MHz
            if (int_value == 20 || int_value == 40 ||
                int_value == 80 || int_value == 160) {
                return DMS_SUCCESS;
            }
            break;

        case WIFI_CONTROL_MODE:
            // 支援常見的 WiFi 模式
            if (strcmp(value, "AP") == 0 || strcmp(value, "STA") == 0 ||
                strcmp(value, "Mesh") == 0 || strcmp(value, "Monitor") == 0) {
                return DMS_SUCCESS;
            }
            break;

        default:
            return DMS_ERROR_UNSUPPORTED;
    }

    return DMS_ERROR_INVALID_PARAMETER;
}


/**
 * @brief 將 DMS 控制命令轉換為 BCML JSON 格式
 */

static cJSON* convert_dms_to_bcml_wireless(const char *item, const char *value) {
    cJSON *root_obj = cJSON_CreateObject();
    cJSON *wireless_obj = cJSON_CreateObject();
    cJSON *radio_array = cJSON_CreateArray();
    cJSON *radio_obj = cJSON_CreateObject();
    cJSON *ssid_array = cJSON_CreateArray();
    cJSON *ssid_obj = cJSON_CreateObject();

    if (!root_obj || !wireless_obj || !radio_array || !radio_obj || !ssid_array || !ssid_obj) {
        if (root_obj) cJSON_Delete(root_obj);
        if (wireless_obj) cJSON_Delete(wireless_obj);
        if (radio_array) cJSON_Delete(radio_array);
        if (radio_obj) cJSON_Delete(radio_obj);
        if (ssid_array) cJSON_Delete(ssid_array);
        if (ssid_obj) cJSON_Delete(ssid_obj);
        return NULL;
    }

    printf("🔄 [BCML] Converting: %s = %s\n", item, value);

    // 添加所有必要的 radio 預設值
    cJSON_AddNumberToObject(radio_obj, "power", 20);
    cJSON_AddNumberToObject(radio_obj, "channel2g", 6);
    cJSON_AddNumberToObject(radio_obj, "channel5g", 149);
    cJSON_AddNumberToObject(radio_obj, "bandwidth2g", 20);
    cJSON_AddNumberToObject(radio_obj, "bandwidth5g", 80);
    cJSON_AddBoolToObject(radio_obj, "dfs", false);
    cJSON_AddBoolToObject(radio_obj, "atf", false);
    cJSON_AddBoolToObject(radio_obj, "bandsteering", false);
    cJSON_AddBoolToObject(radio_obj, "zerowait", false);

    // 添加所有必要的 ssid 預設值
    cJSON_AddStringToObject(ssid_obj, "ssid", "default_ssid");
    cJSON_AddBoolToObject(ssid_obj, "hide", false);
    cJSON_AddNumberToObject(ssid_obj, "security", 2);  // WPA2
    cJSON_AddStringToObject(ssid_obj, "password", "defaultpass");
    cJSON_AddBoolToObject(ssid_obj, "password_onscreen", false);
    cJSON_AddBoolToObject(ssid_obj, "enable2g", true);
    cJSON_AddBoolToObject(ssid_obj, "enable5g", true);
    cJSON_AddBoolToObject(ssid_obj, "isolation", false);
    cJSON_AddBoolToObject(ssid_obj, "hopping", false);

    // 根據實際控制項目覆蓋對應的值
    WiFiControlType_t type = bcml_parse_control_type(item);
    switch (type) {
        case WIFI_CONTROL_CHANNEL_2G:
            cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(radio_obj, "channel2g"), atoi(value));
            break;

        case WIFI_CONTROL_CHANNEL_5G:
            cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(radio_obj, "channel5g"), atoi(value));
            break;

        case WIFI_CONTROL_POWER_2G:
        case WIFI_CONTROL_POWER_5G:
            cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(radio_obj, "power"), atoi(value));
            break;

        case WIFI_CONTROL_BANDWIDTH_2G:
            cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(radio_obj, "bandwidth2g"), atoi(value));
            break;

        case WIFI_CONTROL_BANDWIDTH_5G:
            cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(radio_obj, "bandwidth5g"), atoi(value));
            break;

        default:
            printf("⚠️ [BCML] Unsupported item: %s, using default values\n", item);
            break;
    }

    // 組裝完整的 BCML 期望結構: {"wireless":{"radio":[...],"ssid":[...]}}
    cJSON_AddItemToArray(radio_array, radio_obj);
    cJSON_AddItemToArray(ssid_array, ssid_obj);
    cJSON_AddItemToObject(wireless_obj, "radio", radio_array);
    cJSON_AddItemToObject(wireless_obj, "ssid", ssid_array);
    cJSON_AddItemToObject(root_obj, "wireless", wireless_obj);

    return root_obj;
}



/**
 * @brief 執行 WiFi 控制
 */
int bcml_execute_wifi_control(const char *item, const char *value) {
    if (!item || !value) {
        return DMS_ERROR_INVALID_PARAMETER;
    }

    printf("📡 [BCML] WiFi Control: %s = %s\n", item, value);

    // 解析和驗證參數
    WiFiControlType_t type = bcml_parse_control_type(item);
    if (type == WIFI_CONTROL_UNKNOWN) {
        printf("❌ [BCML] Unknown control item: %s\n", item);
        return DMS_ERROR_UNSUPPORTED;
    }

    // 驗證參數
    int validation_result = bcml_validate_control_params(type, value);
    if (validation_result != DMS_SUCCESS) {
        printf("❌ [BCML] Invalid parameter for %s: %s\n", item, value);
        return validation_result;
    }

    // 轉換為 BCML 格式
    cJSON *wireless_obj = convert_dms_to_bcml_wireless(item, value);
    if (!wireless_obj) {
        printf("❌ [BCML] Failed to convert control parameters\n");
        return DMS_ERROR_JSON_PARSE;
    }

    // 輸出 JSON 用於除錯
    char *json_string = cJSON_PrintUnformatted(wireless_obj);
    if (json_string) {
        printf("📋 [BCML] JSON payload: %s\n", json_string);

        // 使用新的 bcml_config_set API
        bool result = bcml_config_set("wireless", json_string);

        free(json_string);
        cJSON_Delete(wireless_obj);

        if (result) {
            printf("✅ [BCML] WiFi control successful: %s = %s\n", item, value);
            return DMS_SUCCESS;
        } else {
            printf("❌ [BCML] WiFi control failed: %s = %s\n", item, value);
            return DMS_ERROR_MIDDLEWARE_FAILED;
        }
    }

    cJSON_Delete(wireless_obj);
    printf("❌ [BCML] Failed to generate JSON string\n");
    return DMS_ERROR_JSON_PARSE;
}

/**
 * @brief 獲取 WiFi 狀態 (JSON 格式)
 */
int bcml_get_wifi_status(char *status_json, size_t json_size) {
    if (!status_json || json_size == 0) {
        return DMS_ERROR_INVALID_PARAMETER;
    }

    // 確保緩衝區至少 1024 字節
    if (json_size < 1024) {
        printf("⚠️  [BCML] Warning: Small buffer size (%zu), may truncate\n", json_size);
    }

    // 使用新的 bcml_config_get API
    bool result = bcml_config_get("wireless", status_json, json_size);

    if (result) {
        printf("✅ [BCML] WiFi status retrieved successfully\n");
        return DMS_SUCCESS;
    } else {
        printf("❌ [BCML] Failed to get WiFi status\n");
        return DMS_ERROR_MIDDLEWARE_FAILED;
    }
}

/**
 * @brief 執行 WiFi 控制測試序列
 */
int bcml_test_wifi_controls(void) {
    printf("📡 === BCML WiFi Control Test Sequence ===\n");

    int result;

    // 測試 2.4GHz 頻道設定
    printf("🔧 Testing 2.4GHz channel control...\n");
    result = bcml_execute_wifi_control("channel2g", "6");
    if (result != DMS_SUCCESS) {
        printf("❌ 2.4GHz channel test failed\n");
        return result;
    }
    usleep(500000); // 0.5秒延遲

    // 測試 5GHz 頻道設定
    printf("🔧 Testing 5GHz channel control...\n");
    result = bcml_execute_wifi_control("channel5g", "149");
    if (result != DMS_SUCCESS) {
        printf("❌ 5GHz channel test failed\n");
        return result;
    }
    usleep(500000);

    // 測試功率設定
    printf("🔧 Testing power control...\n");
    result = bcml_execute_wifi_control("power2g", "80");
    if (result != DMS_SUCCESS) {
        printf("❌ 2.4GHz power test failed\n");
        return result;
    }

    result = bcml_execute_wifi_control("power5g", "100");
    if (result != DMS_SUCCESS) {
        printf("❌ 5GHz power test failed\n");
        return result;
    }

    printf("✅ === BCML WiFi Control Test Completed Successfully ===\n");
    return DMS_SUCCESS;
}

/**
 * @brief 獲取 BCML 版本資訊
 */
const char *bcml_get_version(void) {
    return "BCML 1.0.0 (bcml_config API)";
}

/*-----------------------------------------------------------*/
/* 內部輔助函數 */

static int validate_channel_value(int channel, bool is5g) {
    if (is5g) {
        // 5GHz 頻道驗證 (常見頻道)
        int valid_5g_channels[] = {36, 40, 44, 48, 52, 56, 60, 64,
                                   100, 104, 108, 112, 116, 120, 124, 128,
                                   132, 136, 140, 144, 149, 153, 157, 161, 165};
        int num_channels = sizeof(valid_5g_channels) / sizeof(valid_5g_channels[0]);

        for (int i = 0; i < num_channels; i++) {
            if (channel == valid_5g_channels[i]) {
                return DMS_SUCCESS;
            }
        }
    } else {
        // 2.4GHz 頻道驗證 (1-14)
        if (channel >= 1 && channel <= 14) {
            return DMS_SUCCESS;
        }
    }

    return DMS_ERROR_INVALID_PARAMETER;
}

static int validate_power_value(int power) {
    // 功率範圍 0-100%
    if (power >= 0 && power <= 100) {
        return DMS_SUCCESS;
    }
    return DMS_ERROR_INVALID_PARAMETER;
}

static const char *control_type_to_string(WiFiControlType_t type) {
    switch (type) {
        case WIFI_CONTROL_CHANNEL_2G:   return "channel2g";
        case WIFI_CONTROL_CHANNEL_5G:   return "channel5g";
        case WIFI_CONTROL_POWER_2G:     return "power2g";
        case WIFI_CONTROL_POWER_5G:     return "power5g";
        case WIFI_CONTROL_BANDWIDTH_2G: return "bandwidth2g";
        case WIFI_CONTROL_BANDWIDTH_5G: return "bandwidth5g";
        case WIFI_CONTROL_MODE:         return "mode";
        case WIFI_CONTROL_UNKNOWN:      return "unknown";
        default:                        return "invalid";
    }
}

#else
// === 模擬版本實作 (當 BCML_MIDDLEWARE_ENABLED 未定義時) ===

/**
 * @brief 初始化 BCML 適配器 (模擬版本)
 */
int bcml_adapter_init(void) {
    printf("🎭 [SIMULATE] BCML adapter initialization (simulation mode)\n");
    return DMS_SUCCESS;
}

/**
 * @brief 清理 BCML 適配器 (模擬版本)
 */
void bcml_adapter_cleanup(void) {
    printf("🎭 [SIMULATE] BCML adapter cleanup (simulation mode)\n");
}

/**
 * @brief 執行 WiFi 控制 (模擬版本)
 */
int bcml_execute_wifi_control(const char *item, const char *value) {
    if (!item || !value) {
        return DMS_ERROR_INVALID_PARAMETER;
    }

    time_t now = time(NULL);
    printf("🎭 [SIMULATE] WiFi Control: %s = %s (timestamp: %ld)\n", item, value, now);

    // 模擬不同控制項目的處理時間
    if (strcmp(item, "channel2g") == 0) {
        printf("   🔄 Simulating 2.4GHz channel change to %s...\n", value);
        usleep(500000); // 0.5秒模擬延遲
    } else if (strcmp(item, "channel5g") == 0) {
        printf("   🔄 Simulating 5GHz channel change to %s...\n", value);
        usleep(800000); // 0.8秒模擬延遲
    } else if (strcmp(item, "power2g") == 0) {
        printf("   🔋 Simulating 2.4GHz power adjustment to %s%%...\n", value);
        usleep(300000); // 0.3秒模擬延遲
    } else if (strcmp(item, "power5g") == 0) {
        printf("   🔋 Simulating 5GHz power adjustment to %s%%...\n", value);
        usleep(300000); // 0.3秒模擬延遲
    } else if (strcmp(item, "bandwidth2g") == 0) {
        printf("   📶 Simulating 2.4GHz bandwidth change to %s MHz...\n", value);
        usleep(400000); // 0.4秒模擬延遲
    } else if (strcmp(item, "bandwidth5g") == 0) {
        printf("   📶 Simulating 5GHz bandwidth change to %s MHz...\n", value);
        usleep(400000); // 0.4秒模擬延遲
    } else {
        printf("   ❓ Simulating unknown control: %s = %s\n", item, value);
        usleep(200000); // 0.2秒模擬延遲
    }

    printf("   ✅ WiFi control simulation completed successfully\n");
    return DMS_SUCCESS;
}

/**
 * @brief 獲取 WiFi 狀態 (模擬版本)
 */
int bcml_get_wifi_status(char *status_json, size_t json_size) {
    if (!status_json || json_size == 0) {
        return DMS_ERROR_INVALID_PARAMETER;
    }

    // 模擬 WiFi 狀態 JSON
    snprintf(status_json, json_size,
        "{"
        "\"radio\":["
        "{"
        "\"channel2g\":6,"
        "\"channel5g\":149,"
        "\"power2g\":80,"
        "\"power5g\":100,"
        "\"bandwidth2g\":40,"
        "\"bandwidth5g\":80,"
        "\"mode\":\"AP\""
        "}"
        "],"
        "\"timestamp\":%ld,"
        "\"simulation\":true"
        "}", time(NULL));

    printf("🎭 [SIMULATE] WiFi status retrieved (simulation)\n");
    return DMS_SUCCESS;
}

/**
 * @brief 獲取 WiFi 狀態 (結構化格式) - 模擬版本
 */
int bcml_get_wifi_status_struct(WiFiStatus_t *status) {
    if (!status) {
        return DMS_ERROR_INVALID_PARAMETER;
    }

    // 填入模擬數據
    status->channel2g = 6;
    status->channel5g = 149;
    status->power2g = 80;
    status->power5g = 100;
    status->bandwidth2g = 40;
    status->bandwidth5g = 80;
    strcpy(status->mode, "AP");
    status->lastUpdated = time(NULL);
    status->isValid = true;

    printf("🎭 [SIMULATE] WiFi status struct retrieved (simulation)\n");
    return DMS_SUCCESS;
}

/**
 * @brief 解析控制項目類型 (模擬版本)
 */
WiFiControlType_t bcml_parse_control_type(const char *item) {
    if (!item) return WIFI_CONTROL_UNKNOWN;

    if (strcmp(item, "channel2g") == 0) {
        return WIFI_CONTROL_CHANNEL_2G;
    } else if (strcmp(item, "channel5g") == 0) {
        return WIFI_CONTROL_CHANNEL_5G;
    } else if (strcmp(item, "power2g") == 0) {
        return WIFI_CONTROL_POWER_2G;
    } else if (strcmp(item, "power5g") == 0) {
        return WIFI_CONTROL_POWER_5G;
    } else if (strcmp(item, "bandwidth2g") == 0) {
        return WIFI_CONTROL_BANDWIDTH_2G;
    } else if (strcmp(item, "bandwidth5g") == 0) {
        return WIFI_CONTROL_BANDWIDTH_5G;
    } else if (strcmp(item, "mode") == 0) {
        return WIFI_CONTROL_MODE;
    }

    return WIFI_CONTROL_UNKNOWN;
}

/**
 * @brief 驗證控制參數 (模擬版本)
 */
int bcml_validate_control_params(WiFiControlType_t type, const char *value) {
    if (!value) return DMS_ERROR_INVALID_PARAMETER;

    int int_value = atoi(value);

    switch (type) {
        case WIFI_CONTROL_CHANNEL_2G:
            // 2.4GHz 頻道 1-14
            if (int_value >= 1 && int_value <= 14) {
                return DMS_SUCCESS;
            }
            break;

        case WIFI_CONTROL_CHANNEL_5G:
            // 5GHz 常見頻道
            if (int_value == 36 || int_value == 40 || int_value == 44 || int_value == 48 ||
                int_value == 149 || int_value == 153 || int_value == 157 || int_value == 161) {
                return DMS_SUCCESS;
            }
            break;

        case WIFI_CONTROL_POWER_2G:
        case WIFI_CONTROL_POWER_5G:
            // 功率 0-100%
            if (int_value >= 0 && int_value <= 100) {
                return DMS_SUCCESS;
            }
            break;

        case WIFI_CONTROL_BANDWIDTH_2G:
            // 2.4GHz 支援 20/40MHz
            if (int_value == 20 || int_value == 40) {
                return DMS_SUCCESS;
            }
            break;

        case WIFI_CONTROL_BANDWIDTH_5G:
            // 5GHz 支援 20/40/80/160MHz
            if (int_value == 20 || int_value == 40 ||
                int_value == 80 || int_value == 160) {
                return DMS_SUCCESS;
            }
            break;

        case WIFI_CONTROL_MODE:
            // 支援常見的 WiFi 模式
            if (strcmp(value, "AP") == 0 || strcmp(value, "STA") == 0 ||
                strcmp(value, "Mesh") == 0 || strcmp(value, "Monitor") == 0) {
                return DMS_SUCCESS;
            }
            break;

        default:
            return DMS_ERROR_UNSUPPORTED;
    }

    printf("🎭 [SIMULATE] Parameter validation failed for %s: %s\n",
           control_type_to_string(type), value);
    return DMS_ERROR_INVALID_PARAMETER;
}

/**
 * @brief 執行 WiFi 控制測試序列 (模擬版本)
 */
int bcml_test_wifi_controls(void) {
    printf("🎭 === BCML WiFi Control Test Sequence (SIMULATION) ===\n");

    int result;

    // 測試 2.4GHz 頻道設定
    printf("🔧 Testing 2.4GHz channel control...\n");
    result = bcml_execute_wifi_control("channel2g", "6");
    if (result != DMS_SUCCESS) {
        printf("❌ 2.4GHz channel test failed\n");
        return result;
    }

    // 測試 5GHz 頻道設定
    printf("🔧 Testing 5GHz channel control...\n");
    result = bcml_execute_wifi_control("channel5g", "149");
    if (result != DMS_SUCCESS) {
        printf("❌ 5GHz channel test failed\n");
        return result;
    }

    // 測試功率設定
    printf("🔧 Testing power control...\n");
    result = bcml_execute_wifi_control("power2g", "80");
    if (result != DMS_SUCCESS) {
        printf("❌ 2.4GHz power test failed\n");
        return result;
    }

    result = bcml_execute_wifi_control("power5g", "100");
    if (result != DMS_SUCCESS) {
        printf("❌ 5GHz power test failed\n");
        return result;
    }

    // 測試頻寬設定
    printf("🔧 Testing bandwidth control...\n");
    result = bcml_execute_wifi_control("bandwidth2g", "40");
    if (result != DMS_SUCCESS) {
        printf("❌ 2.4GHz bandwidth test failed\n");
        return result;
    }

    result = bcml_execute_wifi_control("bandwidth5g", "80");
    if (result != DMS_SUCCESS) {
        printf("❌ 5GHz bandwidth test failed\n");
        return result;
    }

    printf("✅ === BCML WiFi Control Test Completed Successfully (SIMULATION) ===\n");
    return DMS_SUCCESS;
}

/**
 * @brief 獲取 BCML 版本資訊 (模擬版本)
 */
const char *bcml_get_version(void) {
    return "BCML Simulation Mode v1.0.0";
}

// 模擬版本的內部輔助函數
static const char *control_type_to_string(WiFiControlType_t type) {
    switch (type) {
        case WIFI_CONTROL_CHANNEL_2G:   return "channel2g";
        case WIFI_CONTROL_CHANNEL_5G:   return "channel5g";
        case WIFI_CONTROL_POWER_2G:     return "power2g";
        case WIFI_CONTROL_POWER_5G:     return "power5g";
        case WIFI_CONTROL_BANDWIDTH_2G: return "bandwidth2g";
        case WIFI_CONTROL_BANDWIDTH_5G: return "bandwidth5g";
        case WIFI_CONTROL_MODE:         return "mode";
        case WIFI_CONTROL_UNKNOWN:      return "unknown";
        default:                        return "invalid";
    }
}

#endif // BCML_MIDDLEWARE_ENABLED
