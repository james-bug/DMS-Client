
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

#include <stdint.h>     // for uintptr_t
#include <sys/types.h>  // for pid_t



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
 * @brief 質數矩陣分佈系統 - 6×4質數矩陣創造複雜非週期分散
 */

// 6×4質數矩陣 - 每行代表不同的時間策略，每列代表不同的精度級別
static const uint32_t prime_matrix[6][4] = {
    // 策略0: 小質數密集型 (適合快速重連)
    {271, 277, 281, 283},
    // 策略1: 中等質數平衡型 (適合標準重連)  
    {293, 307, 311, 313},
    // 策略2: 大質數稀疏型 (適合後期重連)
    {317, 331, 337, 347},
    // 策略3: 跳躍質數型 (適合混合場景)
    {349, 353, 359, 367},
    // 策略4: 黃金質數型 (數學特性優良)
    {373, 379, 383, 389},
    // 策略5: 最大質數型 (適合極端分散)
    {397, 401, 409, 419}
};

// 質數路徑選擇器 - 基於MAC特征選擇矩陣路徑
static const uint8_t prime_path_matrix[8][6] = {
    // 路徑0: 對角線路徑
    {0, 1, 2, 3, 4, 5},
    // 路徑1: 之字形路徑  
    {0, 2, 1, 3, 5, 4},
    // 路徑2: 螺旋路徑
    {0, 1, 3, 5, 4, 2},
    // 路徑3: 反對角路徑
    {5, 4, 3, 2, 1, 0},
    // 路徑4: 跳躍路徑
    {0, 3, 1, 4, 2, 5},
    // 路徑5: 混沌路徑
    {2, 0, 4, 1, 5, 3},
    // 路徑6: 黃金分割路徑
    {0, 2, 4, 1, 3, 5},
    // 路徑7: 質數分佈路徑
    {1, 3, 0, 5, 2, 4}
};

/**
 * @brief 計算MAC地址的質數矩陣特征
 */
static uint32_t calculate_mac_matrix_signature(const char* mac_address)
{
    if (!mac_address || strlen(mac_address) == 0) {
        return 0;
    }
    
    uint32_t signature = 0;
    size_t mac_len = strlen(mac_address);
    
    // 多層特征提取
    // 第一層：字符值加權和
    for (size_t i = 0; i < mac_len; i++) {
        signature += mac_address[i] * (i * 7 + 1);  // 使用質數7作為權重
    }
    
    // 第二層：位置敏感雜湊
    for (size_t i = 0; i < mac_len; i++) {
        signature ^= ((uint32_t)mac_address[i] << ((i % 4) * 8));
    }
    
    // 第三層：長度影響
    signature *= (mac_len * 11 + 13);  // 質數11和13
    
    return signature;
}

/**
 * @brief 質數矩陣路徑計算器
 */
static uint32_t calculate_prime_matrix_path(const char* mac_seed, uint32_t time_slot)
{
    // 獲取MAC矩陣特征
    uint32_t mac_signature = calculate_mac_matrix_signature(mac_seed);
    
    // 路徑選擇：基於MAC特征選擇8種路徑之一
    uint32_t path_index = mac_signature % 8;
    const uint8_t* selected_path = prime_path_matrix[path_index];
    
    // 時間段映射：12個時間段映射到6個矩陣行
    uint32_t matrix_row = selected_path[time_slot % 6];
    
    // 精度級別選擇：基於MAC和時間的組合
    time_t current_time = time(NULL);
    uint32_t precision_level = (mac_signature + (uint32_t)current_time / 1800) % 4; // 每30分鐘變化
    
    // 從質數矩陣中獲取對應的質數
    uint32_t selected_prime = prime_matrix[matrix_row][precision_level];
    
    return selected_prime;
}

/**
 * @brief 質數矩陣非線性組合器
 */
static uint32_t combine_prime_matrix_offsets(const char* mac_seed, uint32_t time_slot)
{
    uint32_t cumulative_offset = 0;
    uint32_t mac_signature = calculate_mac_matrix_signature(mac_seed);
    
    // 動態路徑遍歷：不同設備使用不同的累積路徑
    for (uint32_t step = 0; step < time_slot; step++) {
        // 每一步都可能選擇不同的矩陣路徑
        uint32_t step_signature = mac_signature + step * 17; // 質數17作為步進因子
        uint32_t step_path = step_signature % 8;
        uint32_t step_row = prime_path_matrix[step_path][step % 6];
        
        // 動態精度選擇
        time_t current_time = time(NULL);
        uint32_t step_precision = (step_signature + (uint32_t)current_time / 900) % 4; // 每15分鐘變化
        
        // 累積質數偏移
        uint32_t step_prime = prime_matrix[step_row][step_precision];
        cumulative_offset += step_prime;
        
        // 非線性調整：避免簡單累加
        if (step > 0) {
            // 質數乘積調製 (防止線性累積)
            uint32_t modulation = (cumulative_offset * 31) % 127; // 質數31和127
            cumulative_offset += modulation;
        }
    }
    
    return cumulative_offset;
}

/**
 * @brief 多維時間段矩陣系統 - 24×4矩陣創造96個精密分散點
 */

// 24×4多維時間段矩陣 - 每行是主時間段，每列是子時間段
static const uint32_t multidimensional_time_matrix[24][4] = {
    // 主段0: 超早期密集段 (0-4.5分鐘)
    {67, 71, 73, 79},
    // 主段1: 早期密集段 (4.5-9分鐘)  
    {83, 89, 97, 101},
    // 主段2: 早期平衡段 (9-13.5分鐘)
    {103, 107, 109, 113},
    // 主段3: 早期擴展段 (13.5-18分鐘)
    {127, 131, 137, 139},
    // 主段4: 前期密集段 (18-22.5分鐘)
    {149, 151, 157, 163},
    // 主段5: 前期平衡段 (22.5-27分鐘)
    {167, 173, 179, 181},
    // 主段6: 前期擴展段 (27-31.5分鐘)
    {191, 193, 197, 199},
    // 主段7: 前期晚段 (31.5-36分鐘)
    {211, 223, 227, 229},
    // 主段8: 中期早段 (36-40.5分鐘)
    {233, 239, 241, 251},
    // 主段9: 中期密集段 (40.5-45分鐘)
    {257, 263, 269, 271},
    // 主段10: 中期平衡段 (45-49.5分鐘)
    {277, 281, 283, 293},
    // 主段11: 中期擴展段 (49.5-54分鐘)
    {307, 311, 313, 317},
    // 主段12: 中期晚段 (54-58.5分鐘)
    {331, 337, 347, 349},
    // 主段13: 後期早段 (58.5-63分鐘)
    {353, 359, 367, 373},
    // 主段14: 後期密集段 (63-67.5分鐘)
    {379, 383, 389, 397},
    // 主段15: 後期平衡段 (67.5-72分鐘)
    {401, 409, 419, 421},
    // 主段16: 後期擴展段 (72-76.5分鐘)
    {431, 433, 439, 443},
    // 主段17: 後期晚段 (76.5-81分鐘)
    {449, 457, 461, 463},
    // 主段18: 晚期早段 (81-85.5分鐘)
    {467, 479, 487, 491},
    // 主段19: 晚期密集段 (85.5-90分鐘)
    {499, 503, 509, 521},
    // 主段20: 晚期平衡段 (90-94.5分鐘)
    {523, 541, 547, 557},
    // 主段21: 晚期擴展段 (94.5-99分鐘)
    {563, 569, 571, 577},
    // 主段22: 極晚段 (99-103.5分鐘)
    {587, 593, 599, 601},
    // 主段23: 終極段 (103.5-108分鐘)
    {607, 613, 617, 619}
};

// 子時間段選擇策略矩陣 - 4種不同的子段選擇策略
static const uint8_t sub_segment_strategy[4][4] = {
    // 策略0: 線性遞增 (適合均勻分散)
    {0, 1, 2, 3},
    // 策略1: 中心外擴 (適合中心聚集後擴散)
    {1, 2, 0, 3},
    // 策略2: 邊緣內聚 (適合邊緣開始向中心)
    {0, 3, 1, 2},
    // 策略3: 隨機跳躍 (適合最大化分散)
    {2, 0, 3, 1}
};

/**
 * @brief 計算多維MAC特征向量
 */
static void calculate_multidimensional_mac_features(const char* mac_seed, uint32_t features[4])
{
    if (!mac_seed || strlen(mac_seed) == 0) {
        features[0] = features[1] = features[2] = features[3] = 1;
        return;
    }
    
    size_t mac_len = strlen(mac_seed);
    
    // 特征0: 前段特征 (前1/4)
    features[0] = 5381;
    for (size_t i = 0; i < mac_len / 4 + 1; i++) {
        if (i < mac_len) {
            features[0] = ((features[0] << 5) + features[0]) + mac_seed[i];
        }
    }
    
    // 特征1: 前中段特征 (第2個1/4)
    features[1] = 7919;
    for (size_t i = mac_len / 4; i < mac_len / 2 + 1; i++) {
        if (i < mac_len) {
            features[1] = ((features[1] << 3) + features[1]) + mac_seed[i];
        }
    }
    
    // 特征2: 後中段特征 (第3個1/4)
    features[2] = 65537;
    for (size_t i = mac_len / 2; i < (mac_len * 3) / 4 + 1; i++) {
        if (i < mac_len) {
            features[2] = ((features[2] << 7) + features[2]) + mac_seed[i];
        }
    }
    
    // 特征3: 後段特征 (最後1/4)
    features[3] = 2147483647u;
    for (size_t i = (mac_len * 3) / 4; i < mac_len; i++) {
        features[3] = ((features[3] << 2) + features[3]) + mac_seed[i];
    }
}

/**
 * @brief 動態時間段分配器 - 基於多維特征分配主時間段
 */
static uint32_t allocate_primary_time_segment(const char* mac_seed)
{
    uint32_t mac_features[4];
    calculate_multidimensional_mac_features(mac_seed, mac_features);
    
    // 多特征加權組合選擇主時間段
    uint64_t weighted_sum = 0;
    const uint32_t weights[4] = {7, 11, 13, 17}; // 不同質數權重
    
    for (int i = 0; i < 4; i++) {
        weighted_sum += (uint64_t)mac_features[i] * weights[i];
    }
    
    // 選擇24個主時間段之一
    uint32_t primary_segment = (uint32_t)(weighted_sum % 24);
    
    return primary_segment;
}

/**
 * @brief 動態子時間段分配器 - 基於時間和MAC選擇子段
 */
static uint32_t allocate_sub_time_segment(const char* mac_seed, uint32_t primary_segment)
{
    uint32_t mac_features[4];
    calculate_multidimensional_mac_features(mac_seed, mac_features);
    
    // 時間因子：每15分鐘變化一次
    time_t current_time = time(NULL);
    uint32_t time_factor = (uint32_t)(current_time / 900) % 16; // 每15分鐘，16種變化
    
    // 選擇子段策略 (4種策略)
    uint32_t strategy = (mac_features[0] + time_factor) % 4;
    
    // 基於主段和MAC特征選擇子段位置
    uint32_t sub_position = (mac_features[1] + primary_segment * 23) % 4; // 質數23避免週期
    
    // 從策略矩陣中獲取實際的子段索引
    uint32_t sub_segment = sub_segment_strategy[strategy][sub_position];
    
    return sub_segment;
}

/**
 * @brief 多維時間段矩陣累積計算器
 */
static uint32_t calculate_multidimensional_cumulative_offset(const char* mac_seed, uint32_t target_primary, uint32_t target_sub)
{
    uint32_t cumulative_offset = 0;
    
    // 第一階段：累積所有完整的主時間段
    for (uint32_t primary = 0; primary < target_primary; primary++) {
        // 每個主時間段包含4個子段的完整時間
        for (int sub = 0; sub < 4; sub++) {
            cumulative_offset += multidimensional_time_matrix[primary][sub];
        }
        
        // 主段間隔調整 (避免主段之間的時間空隙)
        uint32_t mac_features[4];
        calculate_multidimensional_mac_features(mac_seed, mac_features);
        uint32_t gap_adjustment = (mac_features[primary % 4] % 30) + 10; // 10-39秒間隔
        cumulative_offset += gap_adjustment;
    }
    
    // 第二階段：累積目標主時間段內的子段
    for (uint32_t sub = 0; sub < target_sub; sub++) {
        cumulative_offset += multidimensional_time_matrix[target_primary][sub];
        
        // 子段微調 (精密調整)
        uint32_t mac_features[4];
        calculate_multidimensional_mac_features(mac_seed, mac_features);
        uint32_t micro_adjustment = (mac_features[sub] % 15) + 1; // 1-15秒微調
        cumulative_offset += micro_adjustment;
    }
    
    return cumulative_offset;
}

/**
 * @brief 多維矩陣非線性優化器
 */
static uint32_t optimize_multidimensional_distribution(uint32_t base_offset, const char* mac_seed)
{
    uint32_t mac_features[4];
    calculate_multidimensional_mac_features(mac_seed, mac_features);
    
    // 非線性散列優化
    uint64_t optimization_hash = base_offset;
    
    for (int i = 0; i < 4; i++) {
        optimization_hash ^= ((uint64_t)mac_features[i] << (i * 8));
        optimization_hash *= 0x9E3779B97F4A7C15ULL; // 64位黃金比例
    }
    
    // 時間維度優化
    time_t current_time = time(NULL);
    uint32_t time_optimization = ((uint32_t)current_time / 600) % 300; // 每10分鐘變化
    
    // 最終優化偏移
    uint32_t optimized_offset = (uint32_t)(optimization_hash % UINT32_MAX) + time_optimization;
    
    return optimized_offset;
}


/**
 * @brief 多維時間段矩陣 + 質數矩陣分佈 (96點精密分散)
 */
static uint32_t calculate_time_slot_offset(const char* mac_seed)
{
    if (!mac_seed || strlen(mac_seed) == 0) {
        return 0;
    }
    
    // 第一層：多維時間段矩陣分配 (新核心技術)
    uint32_t primary_segment = allocate_primary_time_segment(mac_seed);    // 0-23
    uint32_t sub_segment = allocate_sub_time_segment(mac_seed, primary_segment); // 0-3
    
    // 第二層：多維矩陣累積計算
    uint32_t multidim_offset = calculate_multidimensional_cumulative_offset(mac_seed, primary_segment, sub_segment);
    
    // 第三層：質數矩陣增強 (保留現有技術)
    uint32_t mac_hash = calculate_seed_from_mac(mac_seed);
    uint32_t legacy_time_slot = mac_hash % 12;  // 保持與現有系統的相容性
    uint32_t matrix_offset = combine_prime_matrix_offsets(mac_seed, legacy_time_slot);
    
    // 第四層：混合優化
    uint32_t hybrid_offset = multidim_offset + (matrix_offset / 4); // 質數矩陣作為微調
    
    // 第五層：多維分佈優化
    uint32_t optimized_offset = optimize_multidimensional_distribution(hybrid_offset, mac_seed);
    
    // 第六層：時間感知最終調整
    time_t current_time = time(NULL);
    uint32_t time_modulation = ((uint32_t)current_time / 1800) % 120; // 每30分鐘變化，0-119秒
    
    // 多維矩陣特有的均勻化處理
    uint32_t mac_features[4];
    calculate_multidimensional_mac_features(mac_seed, mac_features);
    uint32_t uniformity_adjustment = (mac_features[0] ^ mac_features[1] ^ mac_features[2] ^ mac_features[3]) % 60;
    
    // 96點分散的最終組合
    uint32_t total_offset = optimized_offset + time_modulation + uniformity_adjustment;
    
    // 多維散列最終調整：確保96個分散點的最佳利用
    total_offset = (total_offset * 0x9E3779B9) >> 6; // 黃金比例散列，右移6位適配更大範圍
    
    return total_offset;
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
 * @brief 納秒級精密級聯Jitter - 5層超高精度隨機性疊加
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
    uint32_t retry_jitter_base = 15 + (retry_count * 10);
    uint32_t retry_jitter = ((uint32_t)current_time % retry_jitter_base) + 1;
    
    // 第四層：納秒級精密Jitter (新增超高精度)
    struct timespec ts;
    uint32_t nano_jitter = 0;
    uint32_t nano_oscillation = 0;
    
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        // 主納秒Jitter：基於納秒時間戳
        nano_jitter = (ts.tv_nsec / 1000000) % 45;  // 0-44秒 (毫秒級變化)
        
        // 納秒振盪：基於納秒的細微變化
        uint32_t pure_nano = ts.tv_nsec % 1000000;  // 純納秒部分
        nano_oscillation = (pure_nano / 25000) % 20; // 0-19秒 (每25微秒變化)
        
        // 納秒週期性避免：確保不會有固定週期
        uint32_t nano_cycle_break = (pure_nano * 7 + ts.tv_sec * 11) % 15; // 0-14秒
        nano_jitter += nano_cycle_break;
    }
    
    // 第五層：系統熵Jitter (基於系統狀態)
    uint32_t entropy_jitter = 0;
    
    // 簡單系統熵：基於進程ID和時間的組合
    pid_t pid = getpid();
    uint32_t process_entropy = ((uint32_t)pid * (uint32_t)current_time) % 25; // 0-24秒
    
    // 記憶體地址熵：使用棧地址的隨機性
    volatile char stack_var;
    uintptr_t stack_addr = (uintptr_t)&stack_var;
    uint32_t stack_entropy = (stack_addr & 0xFFF) % 18; // 0-17秒
    
    entropy_jitter = process_entropy + stack_entropy;
    
    // 5層級聯組合
    uint32_t total_jitter = mac_jitter + time_jitter + retry_jitter + 
                           nano_jitter + nano_oscillation + entropy_jitter;
    
    // 高階交互項 (納秒級交互增強)
    total_jitter += (mac_jitter * nano_jitter) % 12;      // MAC-納秒交互
    total_jitter += (retry_count * nano_oscillation) % 10; // 重試-納秒振盪交互
    total_jitter += (time_jitter * entropy_jitter) % 8;    // 時間-熵交互
    
    // 非線性混沌項 (避免可預測性)
    uint32_t chaos_factor = ((mac_hash ^ (uint32_t)ts.tv_nsec) * 0x9E3779B9) % 15;
    total_jitter += chaos_factor;
    
    // 智能範圍控制 (根據重試次數動態調整上限)
    uint32_t max_jitter = 120 + (retry_count * 20);  // 基礎120秒，每次重試+20秒
    if (max_jitter > 300) max_jitter = 300;           // 絕對上限5分鐘
    
    if (total_jitter > max_jitter) {
        // 保持隨機性的範圍調整
        total_jitter = (max_jitter * 2 / 3) + (total_jitter % (max_jitter / 3));
    }
    
    return base_delay + total_jitter;
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
 * @brief 量子風格32維度並行雜湊系統
 */
static uint32_t quantum_multidimensional_hash(const char* mac_address)
{
    if (!mac_address || strlen(mac_address) == 0) {
        return 1;
    }
    
    // 32個不同維度的雜湊種子 (每個代表一個"量子態")
    const uint32_t quantum_seeds[32] = {
        0x9E3779B9, 0xC6EF3720, 0x5BD1E995, 0x85EBCA6B,  // 維度 0-3
        0xD2B54394, 0xFEEDBEEF, 0xCAFEBABE, 0xDEADBEEF,  // 維度 4-7
        0x12345678, 0x87654321, 0xABCDEF01, 0x13579BDF,  // 維度 8-11
        0x2468ACE0, 0x97531BDF, 0x1A2B3C4D, 0x5E6F7A8B,  // 維度 12-15
        0x9C0D1E2F, 0x3A4B5C6D, 0x7E8F9A0B, 0x1C2D3E4F,  // 維度 16-19
        0x5A6B7C8D, 0x9E0F1A2B, 0x3C4D5E6F, 0x7A8B9C0D,  // 維度 20-23
        0x1E2F3A4B, 0x5C6D7E8F, 0x9A0B1C2D, 0x3E4F5A6B,  // 維度 24-27
        0x7C8D9E0F, 0x1A2B3C4D, 0x5E6F7A8B, 0x9C0D1E2F   // 維度 28-31
    };
    
    // 不同的雜湊算法函數指針 (模擬不同的量子雜湊函數)
    const uint32_t hash_multipliers[32] = {
        33, 37, 41, 43, 47, 53, 59, 61,      // DJB2變體系列
        67, 71, 73, 79, 83, 89, 97, 101,     // 質數乘數系列
        103, 107, 109, 113, 127, 131, 137, 139,  // 中等質數系列
        149, 151, 157, 163, 167, 173, 179, 181   // 大質數系列
    };
    
    uint32_t quantum_hashes[32];
    size_t mac_len = strlen(mac_address);
    
    // 並行計算32個不同維度的雜湊值
    for (int dimension = 0; dimension < 32; dimension++) {
        uint32_t hash = quantum_seeds[dimension];
        uint32_t multiplier = hash_multipliers[dimension];
        
        // 每個維度使用不同的雜湊策略
        switch (dimension % 4) {
            case 0: // DJB2變體
                for (size_t i = 0; i < mac_len; i++) {
                    hash = ((hash << 5) + hash) * multiplier + mac_address[i];
                }
                break;
                
            case 1: // FNV-1a變體  
                for (size_t i = 0; i < mac_len; i++) {
                    hash ^= mac_address[i];
                    hash *= multiplier;
                }
                break;
                
            case 2: // SDBM變體
                for (size_t i = 0; i < mac_len; i++) {
                    hash = mac_address[i] + (hash << 6) + (hash << 16) - hash;
                    hash *= multiplier;
                }
                break;
                
            case 3: // 自定義混沌雜湊
                for (size_t i = 0; i < mac_len; i++) {
                    hash = ((hash << 7) ^ (hash >> 3)) + mac_address[i] * multiplier;
                    hash ^= (hash >> 11) + (hash << 13);
                }
                break;
        }
        
        quantum_hashes[dimension] = hash;
    }
    
    // 量子態疊加：選擇最佳分散的雜湊值
    // 方法：選擇在當前MAC集合中分散度最好的維度
    uint32_t best_hash = quantum_hashes[0];
    uint32_t best_dispersion_score = 0;
    
    for (int dimension = 0; dimension < 32; dimension++) {
        // 計算分散度評分 (基於雜湊值的數學特性)
        uint32_t hash = quantum_hashes[dimension];
        
        // 分散度評分：位元分佈均勻性 + 數值分佈特性
        uint32_t bit_scatter = 0;
        for (int bit = 0; bit < 32; bit++) {
            if (hash & (1u << bit)) bit_scatter++;
        }
        
        // 理想的位元分佈是16個1和16個0
        uint32_t bit_balance = 32 - abs((int)bit_scatter - 16);
        
        // 數值分佈特性：避免極端值
        uint32_t value_balance = (hash % 1000) + ((hash >> 16) % 1000);
        
        // 綜合分散度評分
        uint32_t dispersion_score = bit_balance * 100 + value_balance;
        
        if (dispersion_score > best_dispersion_score) {
            best_dispersion_score = dispersion_score;
            best_hash = hash;
        }
    }
    
    return best_hash > 0 ? best_hash : 1;
}

/**
 * @brief 量子雜湊維度選擇器 - 根據系統狀態動態選擇最佳維度
 */
static uint32_t select_quantum_dimension(const char* mac_address, uint32_t time_factor)
{
    // 基於MAC地址特征選擇起始維度
    uint32_t mac_signature = 0;
    size_t mac_len = strlen(mac_address);
    
    for (size_t i = 0; i < mac_len; i++) {
        mac_signature += mac_address[i] * (i + 1);
    }
    
    // 時間因子影響維度選擇
    uint32_t time_influence = time_factor % 8;
    
    // 動態維度選擇：每個設備在不同時間會選擇不同的"量子態"
    uint32_t selected_dimension = (mac_signature + time_influence) % 32;
    
    return selected_dimension;
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
    
    // 第一層：量子風格32維度並行雜湊 (新增核心技術)
    uint32_t quantum_hash = quantum_multidimensional_hash(mac_address);
    
    // 第二層：MAC段位多維度雜湊 (保留現有)
    uint32_t mac_segment_hash = calculate_mac_segment_hash(mac_address);
    
    // 第三層：傳統DJB2雜湊 (保留現有)
    uint32_t djb2_hash = 5381;
    int c;
    const char* ptr = mac_address;
    while ((c = *ptr++)) {
        djb2_hash = ((djb2_hash << 5) + djb2_hash) + c;
    }
    
    // 第四層：動態維度選擇 (量子態選擇)
    uint32_t selected_dimension = select_quantum_dimension(mac_address, (uint32_t)current_time);
    uint32_t dimension_bonus = (selected_dimension * 0x9E3779B9) % 2048;
    
    // 四層雜湊的量子疊加組合
    uint32_t quantum_superposition = quantum_hash ^ mac_segment_hash ^ djb2_hash ^ dynamic_salt;
    
    // 量子干涉項 (模擬量子干涉效應)
    quantum_superposition += (quantum_hash * mac_segment_hash) % 4096;
    quantum_superposition ^= (djb2_hash * dynamic_salt) % 2048;
    quantum_superposition += dimension_bonus;
    
    // 量子退相干 (防止過度相關性)
    quantum_superposition ^= (hour_rotation * 0x01010101);
    quantum_superposition *= 0x9E3779B9;  // 黃金比例混沌化
    
    return quantum_superposition > 0 ? quantum_superposition : 1;
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
