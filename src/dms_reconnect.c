
/*
 * DMS Reconnect Module Implementation
 *
 * 重連策略模組實作 - 提取自 dms_client.c
 * 保持與原始 attemptReconnection() 函數完全相同的邏輯
 */

#include "dms_reconnect.h"
#include "dms_log.h"
#include "demo_config.h"

#include <unistd.h>     // for sleep()
#include <time.h>       // for time()
#include <string.h>     // for strncpy()
#include <stdio.h>      // for printf()
#include <stdlib.h>     // for EXIT_SUCCESS

/*-----------------------------------------------------------*/
/* 常數定義 - ✅ 使用 demo_config.h 中已定義的常數，避免重複定義 */

/* MAC_SEED_MULTIPLIER 和 MAC_SEED_MAX_OFFSET 已在 demo_config.h 中定義 */
/* 不需要重複定義，直接使用即可 */

/*-----------------------------------------------------------*/
/* 內部狀態管理（對應原始的 g_reconnectState） */

typedef struct {
    dms_reconnect_state_t state;         // 連接狀態
    uint32_t retry_count;                // 重試次數
    uint32_t total_reconnects;           // 總重連次數
    uint32_t next_retry_delay_seconds;   // 下次重試延遲
    uint32_t last_connect_time;          // 最後連接時間
    char mac_address_seed[32];           // MAC 地址種子
    uint32_t seed_value;                 // 計算出的數字 seed

    // 配置 - ✅ 與 dms_config.h 結構對應
    uint32_t max_retry_attempts;         // 最大重試次數
    uint32_t base_delay_seconds;         // 基礎延遲時間
    uint32_t max_delay_seconds;          // 最大延遲時間

    // 依賴注入的介面
    dms_reconnect_interface_t interface;

    bool initialized;
} dms_reconnect_context_t;

static dms_reconnect_context_t g_reconnect_ctx = {0};

/*-----------------------------------------------------------*/
/* 內部函數宣告 */

static uint32_t calculate_backoff_delay_with_seed(uint32_t retry_count, const char* mac_seed);
static uint32_t calculate_seed_from_mac(const char* mac_address);
static void initialize_mac_address_seed(void);

/*-----------------------------------------------------------*/
/* 公開介面函數實作 */

/**
 * @brief 初始化重連模組
 */
dms_result_t dms_reconnect_init(const dms_reconnect_config_t* config)
{
    if (!config) {
        DMS_LOG_ERROR("Invalid reconnect configuration");
        return DMS_ERROR_INVALID_PARAMETER;
    }

    DMS_LOG_INFO("Initializing DMS reconnect module...");

    // 清空內部狀態
    memset(&g_reconnect_ctx, 0, sizeof(g_reconnect_ctx));

    // 設定配置 - ✅ 直接對應 dms_config_get_reconnect() 結構
    g_reconnect_ctx.max_retry_attempts = config->max_retry_attempts;
    g_reconnect_ctx.base_delay_seconds = config->base_delay_seconds;
    g_reconnect_ctx.max_delay_seconds = config->max_delay_seconds;

    // 初始化狀態 - ✅ 與原始 g_reconnectState 初始化相同
    g_reconnect_ctx.state = CONNECTION_STATE_DISCONNECTED;
    g_reconnect_ctx.retry_count = 0;
    g_reconnect_ctx.next_retry_delay_seconds = config->base_delay_seconds;
    g_reconnect_ctx.total_reconnects = 0;
    g_reconnect_ctx.last_connect_time = 0;

    // 初始化 MAC 地址種子 - ✅ 對應原始的 initializeMacAddressSeed()
    initialize_mac_address_seed();

    g_reconnect_ctx.initialized = true;

    DMS_LOG_INFO("✅ DMS reconnect module initialized successfully");
    DMS_LOG_DEBUG("Config: max_attempts=%u, base_delay=%u, max_delay=%u",
                  config->max_retry_attempts, config->base_delay_seconds, config->max_delay_seconds);

    return DMS_SUCCESS;
}

/**
 * @brief 註冊重連介面
 */
void dms_reconnect_register_interface(const dms_reconnect_interface_t* interface)
{
    if (!interface) {
        DMS_LOG_ERROR("Invalid reconnect interface");
        return;
    }

    if (!g_reconnect_ctx.initialized) {
        DMS_LOG_ERROR("Reconnect module not initialized");
        return;
    }

    g_reconnect_ctx.interface = *interface;

    DMS_LOG_INFO("✅ Reconnect interface registered successfully");
    DMS_LOG_DEBUG("Interface: connect=%p, disconnect=%p, restart_shadow=%p",
                  interface->connect, interface->disconnect, interface->restart_shadow);
}

/**
 * @brief 執行重連嘗試 - ✅ 對應原始的 attemptReconnection()
 */
dms_result_t dms_reconnect_attempt(void)
{
    if (!g_reconnect_ctx.initialized) {
        DMS_LOG_ERROR("Reconnect module not initialized");
        return DMS_ERROR_INVALID_PARAMETER;
    }

    DMS_LOG_INFO("🔄 Attempting reconnection (attempt %u/%u)...",
                 g_reconnect_ctx.retry_count + 1,
                 g_reconnect_ctx.max_retry_attempts);

    g_reconnect_ctx.state = CONNECTION_STATE_RECONNECTING;

    /* 1. 斷開現有連接 - ✅ 對應原始邏輯 */
    if (g_reconnect_ctx.interface.disconnect) {
        g_reconnect_ctx.interface.disconnect();
        DMS_LOG_DEBUG("Existing connection disconnected");
    }

    /* 2. 延遲重連（如果不是第一次）- ✅ 對應原始邏輯 */
    if (g_reconnect_ctx.retry_count > 0) {
        DMS_LOG_INFO("⏳ Waiting %u seconds before reconnection...",
                     g_reconnect_ctx.next_retry_delay_seconds);
        sleep(g_reconnect_ctx.next_retry_delay_seconds);
    }

    /* 3. 重新建立連接 - ✅ 對應原始邏輯 */
    if (!g_reconnect_ctx.interface.connect) {
        DMS_LOG_ERROR("Connect function not registered");
        dms_reconnect_update_failure();
        return DMS_ERROR_INVALID_PARAMETER;  // ✅ 使用 demo_config.h 中實際存在的錯誤碼
    }

    if (g_reconnect_ctx.interface.connect() == DMS_SUCCESS) {
        DMS_LOG_INFO("✅ AWS IoT reconnection successful");

        /* 4. 重啟 Shadow 服務 - ✅ 對應原始邏輯 */
        if (g_reconnect_ctx.interface.restart_shadow) {
            if (g_reconnect_ctx.interface.restart_shadow() == DMS_SUCCESS) {
                DMS_LOG_INFO("✅ Shadow service restarted successfully");
                dms_reconnect_reset_state();
                return DMS_SUCCESS;
            } else {
                DMS_LOG_WARN("⚠️ Reconnection successful but Shadow restart failed");
                // 仍然算成功，因為連接已建立 - ✅ 對應原始邏輯
                dms_reconnect_reset_state();
                return DMS_SUCCESS;
            }
        } else {
            // 沒有 Shadow 重啟函數，直接成功
            dms_reconnect_reset_state();
            return DMS_SUCCESS;
        }
    } else {
        DMS_LOG_ERROR("❌ AWS IoT reconnection failed");
        dms_reconnect_update_failure();
        return DMS_ERROR_UNKNOWN;  // ✅ 使用 demo_config.h 中實際存在的錯誤碼
    }
}

/**
 * @brief 檢查是否應該重連
 */
bool dms_reconnect_should_retry(void)
{
    if (!g_reconnect_ctx.initialized) {
        return false;
    }

    return (g_reconnect_ctx.retry_count < g_reconnect_ctx.max_retry_attempts);
}

/**
 * @brief 獲取下次重連延遲時間 - ✅ 對應原始的指數退避算法
 */
uint32_t dms_reconnect_get_next_delay(void)
{
    if (!g_reconnect_ctx.initialized) {
        return 0;
    }

    return calculate_backoff_delay_with_seed(g_reconnect_ctx.retry_count,
                                           g_reconnect_ctx.mac_address_seed);
}

/**
 * @brief 重設重連狀態 - ✅ 對應原始成功邏輯
 */
void dms_reconnect_reset_state(void)
{
    if (!g_reconnect_ctx.initialized) {
        return;
    }

    g_reconnect_ctx.state = CONNECTION_STATE_CONNECTED;
    g_reconnect_ctx.retry_count = 0;
    g_reconnect_ctx.next_retry_delay_seconds = g_reconnect_ctx.base_delay_seconds;
    g_reconnect_ctx.last_connect_time = (uint32_t)time(NULL);
    g_reconnect_ctx.total_reconnects++;

    DMS_LOG_INFO("🎯 Connection restored successfully");
    DMS_LOG_INFO("   Total reconnects: %u", g_reconnect_ctx.total_reconnects);
}

/**
 * @brief 更新重連失敗狀態 - ✅ 對應原始失敗邏輯
 */
void dms_reconnect_update_failure(void)
{
    if (!g_reconnect_ctx.initialized) {
        return;
    }

    g_reconnect_ctx.state = CONNECTION_STATE_ERROR;
    g_reconnect_ctx.retry_count++;

    // 計算下次延遲 - ✅ 使用原始的指數退避算法
    g_reconnect_ctx.next_retry_delay_seconds = calculate_backoff_delay_with_seed(
        g_reconnect_ctx.retry_count, g_reconnect_ctx.mac_address_seed);

    DMS_LOG_ERROR("❌ Reconnection failed (attempt %u/%u)",
                  g_reconnect_ctx.retry_count, g_reconnect_ctx.max_retry_attempts);

    if (g_reconnect_ctx.retry_count >= g_reconnect_ctx.max_retry_attempts) {
        DMS_LOG_ERROR("💀 Maximum reconnection attempts reached, giving up");
        g_reconnect_ctx.state = CONNECTION_STATE_ERROR;
    }
}

/**
 * @brief 獲取當前重連狀態
 */
dms_reconnect_state_t dms_reconnect_get_state(void)
{
    if (!g_reconnect_ctx.initialized) {
        return CONNECTION_STATE_DISCONNECTED;
    }

    return g_reconnect_ctx.state;
}

/**
 * @brief 獲取重連統計資訊
 */
void dms_reconnect_get_stats(uint32_t* retry_count, uint32_t* total_reconnects)
{
    if (!g_reconnect_ctx.initialized) {
        if (retry_count) *retry_count = 0;
        if (total_reconnects) *total_reconnects = 0;
        return;
    }

    if (retry_count) {
        *retry_count = g_reconnect_ctx.retry_count;
    }

    if (total_reconnects) {
        *total_reconnects = g_reconnect_ctx.total_reconnects;
    }
}

/**
 * @brief 清理重連模組
 */
void dms_reconnect_cleanup(void)
{
    if (g_reconnect_ctx.initialized) {
        DMS_LOG_INFO("Cleaning up DMS reconnect module...");
        memset(&g_reconnect_ctx, 0, sizeof(g_reconnect_ctx));
        DMS_LOG_INFO("✅ DMS reconnect module cleaned up");
    }
}

/*-----------------------------------------------------------*/
/* 內部函數實作 - ✅ 完全對應原始函數 */

/**
 * @brief 計算帶 MAC 種子的指數退避延遲 - ✅ 對應原始 calculateBackoffDelayWithSeed()
 */
static uint32_t calculate_backoff_delay_with_seed(uint32_t retry_count, const char* mac_seed)
{
    /* 指數退避基礎延遲 - ✅ 與原始邏輯相同 */
    uint32_t base_delay = g_reconnect_ctx.base_delay_seconds * (1 << retry_count);

    /* 從 MAC 地址計算隨機偏移 - ✅ 與原始邏輯相同 */
    uint32_t seed = calculate_seed_from_mac(mac_seed);
    uint32_t random_offset = (seed % MAC_SEED_MAX_OFFSET) * MAC_SEED_MULTIPLIER;

    /* 計算最終延遲時間，確保不超過最大值 - ✅ 與原始邏輯相同 */
    uint32_t final_delay = base_delay + random_offset;
    if (final_delay > g_reconnect_ctx.max_delay_seconds) {
        final_delay = g_reconnect_ctx.max_delay_seconds;
    }

    DMS_LOG_DEBUG("Backoff calculation: retry=%u, base=%u, offset=%u, final=%u",
                  retry_count, base_delay, random_offset, final_delay);

    return final_delay;
}

/**
 * @brief 從 MAC 地址計算種子值 - ✅ 對應原始 calculateSeedFromMac()
 */
static uint32_t calculate_seed_from_mac(const char* mac_address)
{
    if (!mac_address || strlen(mac_address) == 0) {
        return 1; // 預設種子值
    }

    uint32_t seed = 0;
    for (int i = 0; mac_address[i] != '\0'; i++) {
        seed += (uint32_t)mac_address[i];
    }

    return seed > 0 ? seed : 1; // 確保種子不為 0
}

/**
 * @brief 初始化 MAC 地址種子 - ✅ 對應原始 initializeMacAddressSeed()
 */
static void initialize_mac_address_seed(void)
{
    // ✅ 與原始邏輯相同：從 CLIENT_IDENTIFIER 提取 MAC 地址
    const char* client_id = CLIENT_IDENTIFIER;

    if (client_id && strlen(client_id) >= DMS_CLIENT_ID_PREFIX_LENGTH + DMS_MAC_SUFFIX_LENGTH) {
        // 提取 MAC 地址部分（最後 12 個字符）
        const char* mac_part = client_id + strlen(client_id) - DMS_MAC_SUFFIX_LENGTH;
        strncpy(g_reconnect_ctx.mac_address_seed, mac_part, sizeof(g_reconnect_ctx.mac_address_seed) - 1);
        g_reconnect_ctx.mac_address_seed[sizeof(g_reconnect_ctx.mac_address_seed) - 1] = '\0';

        // 計算數字種子
        g_reconnect_ctx.seed_value = calculate_seed_from_mac(g_reconnect_ctx.mac_address_seed);

        DMS_LOG_INFO("MAC address seed initialized: %s (seed value: %u)",
                     g_reconnect_ctx.mac_address_seed, g_reconnect_ctx.seed_value);
    } else {
        // 使用預設種子
        strncpy(g_reconnect_ctx.mac_address_seed, "DEFAULT", sizeof(g_reconnect_ctx.mac_address_seed) - 1);
        g_reconnect_ctx.seed_value = 12345; // 預設種子值

        DMS_LOG_WARN("Using default MAC address seed: %s", g_reconnect_ctx.mac_address_seed);
    }
}
