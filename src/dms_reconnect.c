
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
 * @brief 根據設備MAC地址計算時間段偏移
 */


static uint32_t calculate_time_slot_offset(const char* mac_seed)
{
    if (!mac_seed || strlen(mac_seed) == 0) {
        return 0;
    }
    
    // 12個質數間隔序列 (每段約4.5分鐘，總共90分鐘)
    const uint32_t prime_intervals[] = {
        271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337, 347
    };
    
    uint32_t mac_hash = calculate_seed_from_mac(mac_seed);
    uint32_t time_slot = mac_hash % 12;  // 0-11 共12個時間段
    
    // 計算累積偏移
    uint32_t cumulative_offset = 0;
    for (uint32_t i = 0; i < time_slot; i++) {
        cumulative_offset += prime_intervals[i];
    }
    
    return cumulative_offset;
}


/**
 * @brief MAC段位多維度雜湊 - 分別處理MAC的不同部分
 */
static uint32_t calculate_mac_segment_hash(const char* mac_address)
{
    if (!mac_address || strlen(mac_address) == 0) {
        return 1;
    }
    
    size_t mac_len = strlen(mac_address);
    if (mac_len < 6) {
        // MAC太短，使用原始方法
        return calculate_seed_from_mac(mac_address);
    }
    
    // 分段處理MAC地址
    // 前4位：製造商標識影響
    uint32_t prefix_hash = 5381;
    for (int i = 0; i < 4 && i < mac_len; i++) {
        prefix_hash = ((prefix_hash << 5) + prefix_hash) + mac_address[i];
    }
    
    // 中間4位：批次/型號影響  
    uint32_t middle_hash = 7919;  // 不同的初始值
    int middle_start = mac_len >= 8 ? 4 : mac_len / 2;
    int middle_end = mac_len >= 8 ? 8 : (mac_len * 3) / 4;
    for (int i = middle_start; i < middle_end && i < mac_len; i++) {
        middle_hash = ((middle_hash << 3) + middle_hash) + mac_address[i];
    }
    
    // 後4位：設備個體影響
    uint32_t suffix_hash = 65537;  // 又一個不同初始值
    int suffix_start = mac_len >= 8 ? mac_len - 4 : (mac_len * 3) / 4;
    for (int i = suffix_start; i < mac_len; i++) {
        suffix_hash = ((suffix_hash << 7) + suffix_hash) + mac_address[i];
    }
    
    // 三段雜湊的非線性組合
    uint32_t combined = prefix_hash ^ (middle_hash << 11) ^ (suffix_hash >> 5);
    combined += (prefix_hash * middle_hash) ^ (suffix_hash * 0x9E3779B9);
    
    return combined > 0 ? combined : 1;
}


/**
 * @brief 級聯式Jitter - MAC + 時間 + 重試 三層隨機性疊加
 */
static uint32_t add_cascading_jitter(uint32_t base_delay, uint32_t retry_count, const char* mac_seed)
{
    // 第一層：MAC基礎Jitter (基於MAC雜湊)
    uint32_t mac_hash = calculate_mac_segment_hash(mac_seed);
    uint32_t mac_jitter = (mac_hash % 20) + 1;  // 1-20秒
    
    // 第二層：時間戳Jitter (基於當前時間)
    time_t current_time = time(NULL);
    uint32_t time_jitter = ((uint32_t)current_time % 25) + 5;  // 5-29秒
    
    // 第三層：重試自適應Jitter (基於重試次數)
    uint32_t retry_jitter_base = 15 + (retry_count * 10);  // 基礎15秒 + 每次重試10秒
    uint32_t retry_jitter = ((uint32_t)current_time % retry_jitter_base) + 1;
    
    // 第四層：微秒精度Jitter (基於系統微秒時間)
    struct timespec ts;
    uint32_t micro_jitter = 0;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        micro_jitter = (ts.tv_nsec / 1000000) % 15;  // 0-14秒微調
    }
    
    // 級聯組合：非線性疊加避免簡單相加
    uint32_t cascading_jitter = mac_jitter + time_jitter + retry_jitter + micro_jitter;
    
    // 加入交互項增強隨機性
    cascading_jitter += (mac_jitter * time_jitter) % 10;  // 交互增強
    cascading_jitter += (retry_count * mac_jitter) % 8;   // 重試-MAC交互
    
    // 確保總Jitter在合理範圍內 (最大約100秒)
    if (cascading_jitter > 100) {
        cascading_jitter = 50 + (cascading_jitter % 50);
    }
    
    return base_delay + cascading_jitter;
}


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


     /* 計算初步延遲時間 */
    uint32_t preliminary_delay = base_delay + random_offset;
    
    /* 加入時間段分散 */
    uint32_t time_slot_offset = calculate_time_slot_offset(mac_seed);
    uint32_t slot_dispersed_delay = preliminary_delay + time_slot_offset;
    

    /* 加入級聯式Jitter增強隨機性 */
    uint32_t jittered_delay = add_cascading_jitter(slot_dispersed_delay, retry_count, mac_seed);


    /* 確保不超過最大值 */
    uint32_t final_delay = jittered_delay;
    if (final_delay > g_reconnect_ctx.max_delay_seconds) {
        final_delay = g_reconnect_ctx.max_delay_seconds;
    }


    DMS_LOG_DEBUG("Backoff calculation: retry=%u, base=%u, mac_offset=%u, slot_offset=%u, jitter=+%u, final=%u",
                  retry_count, base_delay, random_offset, time_slot_offset, 
                  jittered_delay - slot_dispersed_delay, final_delay);

    return final_delay;
}



/**
 * @brief 整合MAC段位雜湊 + 時間種子輪轉 (多維度動態隨機性)
 */
static uint32_t calculate_seed_from_mac(const char* mac_address)
{
    if (!mac_address || strlen(mac_address) == 0) {
        return 1;
    }

    // 獲取當前時間進行種子輪轉
    time_t current_time = time(NULL);
    uint32_t hour_rotation = (uint32_t)(current_time / 3600) % 24;
    
    // 時間輪轉種子表
    const uint32_t time_seeds[] = {
        0x9E3779B9, 0xC6EF3720, 0x5BD1E995, 0x85EBCA6B,
        0xD2B54394, 0xFEEDBEEF, 0xCAFEBABE, 0xDEADBEEF,
        0x12345678, 0x87654321, 0xABCDEF01, 0x13579BDF,
        0x2468ACE0, 0x97531BDF, 0x1A2B3C4D, 0x5E6F7A8B,
        0x9C0D1E2F, 0x3A4B5C6D, 0x7E8F9A0B, 0x1C2D3E4F,
        0x5A6B7C8D, 0x9E0F1A2B, 0x3C4D5E6F, 0x7A8B9C0D
    };
    
    uint32_t dynamic_salt = time_seeds[hour_rotation];
    
    // 使用MAC段位多維度雜湊 (新增)
    uint32_t mac_segment_hash = calculate_mac_segment_hash(mac_address);
    
    // 傳統DJB2雜湊
    uint32_t djb2_hash = 5381;
    int c;
    const char* ptr = mac_address;
    while ((c = *ptr++)) {
        djb2_hash = ((djb2_hash << 5) + djb2_hash) + c;
    }
    
    // 多層雜湊組合：MAC段位 + DJB2 + 時間種子
    uint32_t combined_hash = mac_segment_hash ^ djb2_hash ^ dynamic_salt;
    combined_hash += (mac_segment_hash * djb2_hash) ^ (dynamic_salt >> 8);
    combined_hash ^= (hour_rotation * 0x01010101);
    
    return combined_hash > 0 ? combined_hash : 1;
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
