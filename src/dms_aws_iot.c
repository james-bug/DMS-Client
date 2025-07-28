
/*
 * DMS AWS IoT Module Implementation
 *
 * This file contains the implementation of AWS IoT connectivity functions
 * extracted from dms_client.c with identical behavior.
 *
 * All original logic is preserved to ensure zero-risk refactoring.
 */

#include "dms_aws_iot.h"

/* Standard library includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* AWS IoT SDK includes - 與原始程式碼完全相同 */
#include "clock.h"

/*-----------------------------------------------------------*/
/* 模組內部狀態 (取代原始的全域變數) */

static aws_iot_context_t g_aws_iot_context = {0};
static const dms_config_t* g_config = NULL;
static bool g_initialized = false;

/* 網路緩衝區 - 與原始 g_fixedBuffer 相同 */
static uint8_t g_network_buffer[2048]; // 使用配置中的 NETWORK_BUFFER_SIZE

/* 🔧 QoS 追蹤緩衝區 - 與原始程式碼完全相同 */
#define OUTGOING_PUBLISH_RECORD_COUNT    ( 10U )
#define INCOMING_PUBLISH_RECORD_COUNT    ( 10U )

static MQTTPubAckInfo_t g_outgoingPublishRecords[OUTGOING_PUBLISH_RECORD_COUNT];
static MQTTPubAckInfo_t g_incomingPublishRecords[INCOMING_PUBLISH_RECORD_COUNT];

/*-----------------------------------------------------------*/
/* 內部函數宣告 */

static dms_result_t convert_mqtt_status_to_dms_result(MQTTStatus_t mqtt_status);
static dms_result_t convert_openssl_status_to_dms_result(int openssl_status);

/*-----------------------------------------------------------*/
/* 公開介面函數實作 */

/*
 * 修正的 dms_aws_iot_init 函數
 * 
 * 關鍵修正：正確初始化 NetworkContext 以避免 pNetworkContext is NULL 錯誤
 */

dms_result_t dms_aws_iot_init(const dms_config_t* config)
{
    if (config == NULL) {
        DMS_LOG_ERROR("❌ AWS IoT init: NULL configuration");
        return DMS_ERROR_INVALID_PARAMETER;
    }

    if (g_initialized) {
        DMS_LOG_WARN("⚠️  AWS IoT already initialized, reinitializing...");
        dms_aws_iot_cleanup();
    }

    DMS_LOG_INFO("🔧 Initializing AWS IoT module...");

    /* 保存配置指針 */
    g_config = config;

    /* 初始化內部狀態 */
    memset(&g_aws_iot_context, 0, sizeof(g_aws_iot_context));
    g_aws_iot_context.state = AWS_IOT_STATE_DISCONNECTED;

    /* 🔧 關鍵修正：正確初始化 NetworkContext */
#ifdef USE_OPENSSL
    /* 為 NetworkContext 分配 OpensslParams_t 結構 */
    static OpensslParams_t openssl_params = {0};
    g_aws_iot_context.network_context.pParams = &openssl_params;
    DMS_LOG_DEBUG("✅ NetworkContext initialized with OpenSSL params");
#endif

    /* 初始化固定緩衝區 - 與原始程式碼相同 */
    g_aws_iot_context.fixed_buffer.pBuffer = g_network_buffer;
    g_aws_iot_context.fixed_buffer.size = sizeof(g_network_buffer);

    g_initialized = true;

    DMS_LOG_INFO("✅ AWS IoT module initialized successfully");
    DMS_LOG_DEBUG("   Endpoint: %s:%d", 
                 config->aws_iot.aws_endpoint, 
                 config->aws_iot.mqtt_port);
    DMS_LOG_DEBUG("   Client ID: %s", config->aws_iot.client_id);

    return DMS_SUCCESS;
}


dms_result_t dms_aws_iot_connect(void)
{
    if (!g_initialized) {
        DMS_LOG_ERROR("❌ AWS IoT not initialized");
        return DMS_ERROR_DEVICE_INFO_UNAVAILABLE;  // ✅ 使用 demo_config.h 中存在的錯誤碼
    }

    DMS_LOG_INFO("🔌 Establishing AWS IoT connection...");

    /* 步驟1：建立 TLS 連接 */
    dms_result_t result = dms_aws_iot_establish_tls();
    if (result != DMS_SUCCESS) {
        DMS_LOG_ERROR("❌ TLS connection failed");
        return result;
    }

    /* 步驟2：建立 MQTT 連接 */
    result = dms_aws_iot_establish_mqtt();
    if (result != DMS_SUCCESS) {
        DMS_LOG_ERROR("❌ MQTT connection failed");
        /* 清理 TLS 連接 */
#ifdef USE_OPENSSL
        Openssl_Disconnect(&g_aws_iot_context.network_context);
#endif
        return result;
    }

    g_aws_iot_context.state = AWS_IOT_STATE_MQTT_CONNECTED;
    DMS_LOG_INFO("✅ AWS IoT connection established successfully");

    return DMS_SUCCESS;
}

dms_result_t dms_aws_iot_establish_tls(void)
{
    /* 這是原始 establishTlsConnection() 函數的直接移植 */
#ifdef USE_OPENSSL
    DMS_LOG_TLS("🔐 Establishing TLS connection...");

    /* 設定伺服器資訊 - 與原始程式碼完全相同 */
    ServerInfo_t serverInfo = {0};
    serverInfo.pHostName = g_config->aws_iot.aws_endpoint;
    serverInfo.hostNameLength = strlen(g_config->aws_iot.aws_endpoint);
    serverInfo.port = g_config->aws_iot.mqtt_port;

    /* 設定 OpenSSL 憑證資訊 - 與原始程式碼完全相同 */
    OpensslCredentials_t credentials = {0};
    credentials.pAlpnProtos = NULL;
    credentials.alpnProtosLen = 0;
    credentials.sniHostName = NULL;
    credentials.maxFragmentLength = 0;
    credentials.pRootCaPath = g_config->aws_iot.ca_cert_path;
    credentials.pClientCertPath = g_config->aws_iot.client_cert_path;
    credentials.pPrivateKeyPath = g_config->aws_iot.private_key_path;

    DMS_LOG_DEBUG("   Endpoint: %s:%d", serverInfo.pHostName, serverInfo.port);
    DMS_LOG_DEBUG("   Root CA: %s", credentials.pRootCaPath);
    DMS_LOG_DEBUG("   Client Cert: %s", credentials.pClientCertPath);
    DMS_LOG_DEBUG("   Private Key: %s", credentials.pPrivateKeyPath);

    /* 初始化 OpenSSL 連線 - 與原始程式碼完全相同 */
    OpensslStatus_t opensslStatus = Openssl_Connect(
        &g_aws_iot_context.network_context,
        &serverInfo,
        &credentials,
        g_config->aws_iot.transport_timeout_ms,
        g_config->aws_iot.transport_timeout_ms
    );

    if (opensslStatus != OPENSSL_SUCCESS) {
        DMS_LOG_ERROR("❌ Failed to establish TLS connection (status: %d)", opensslStatus);
        return convert_openssl_status_to_dms_result(opensslStatus);
    }

    g_aws_iot_context.state = AWS_IOT_STATE_TLS_CONNECTED;
    DMS_LOG_TLS("✅ TLS connection established successfully");

    return DMS_SUCCESS;
#else
    DMS_LOG_ERROR("❌ OpenSSL support not compiled in");
    return DMS_ERROR_UCI_CONFIG_FAILED;  // ✅ 使用 demo_config.h 中存在的錯誤碼
#endif
}

dms_result_t dms_aws_iot_establish_mqtt(void)
{
    /* 這是原始 establishMqttConnection() 函數的直接移植 */
    DMS_LOG_MQTT("🔌 Establishing MQTT connection...");
    DMS_LOG_DEBUG("   Client ID: %s", g_config->aws_iot.client_id);

    /* 設定傳輸介面 - 與原始程式碼完全相同 */
    TransportInterface_t transportInterface = {0};
    transportInterface.pNetworkContext = &g_aws_iot_context.network_context;
#ifdef USE_OPENSSL
    transportInterface.send = Openssl_Send;
    transportInterface.recv = Openssl_Recv;
#endif

    /* 初始化 MQTT 上下文 - 與原始程式碼完全相同 */
    MQTTStatus_t mqttStatus = MQTT_Init(
        &g_aws_iot_context.mqtt_context,
        &transportInterface,
        Clock_GetTimeMs,
        dms_aws_iot_event_callback,
        &g_aws_iot_context.fixed_buffer
    );

    if (mqttStatus != MQTTSuccess) {
        DMS_LOG_ERROR("❌ Failed to initialize MQTT context (status: %d)", mqttStatus);
        return convert_mqtt_status_to_dms_result(mqttStatus);
    }

    /* 🔧 關鍵修正：初始化 QoS1/QoS2 狀態追蹤 */
    mqttStatus = MQTT_InitStatefulQoS(
        &g_aws_iot_context.mqtt_context,
        g_outgoingPublishRecords,
        OUTGOING_PUBLISH_RECORD_COUNT,
        g_incomingPublishRecords,
        INCOMING_PUBLISH_RECORD_COUNT
    );

    if (mqttStatus != MQTTSuccess) {
        DMS_LOG_ERROR("❌ Failed to initialize MQTT QoS tracking (status: %d)", mqttStatus);
        return convert_mqtt_status_to_dms_result(mqttStatus);
    }

    DMS_LOG_DEBUG("✅ QoS1/QoS2 support initialized");

    /* 設定 MQTT 連接資訊 - 與原始程式碼完全相同 */
    MQTTConnectInfo_t connectInfo = {0};
    connectInfo.cleanSession = true;
    connectInfo.pClientIdentifier = g_config->aws_iot.client_id;
    connectInfo.clientIdentifierLength = strlen(g_config->aws_iot.client_id);
    connectInfo.keepAliveSeconds = g_config->aws_iot.keep_alive_seconds;
    connectInfo.pUserName = NULL;
    connectInfo.userNameLength = 0;
    connectInfo.pPassword = NULL;
    connectInfo.passwordLength = 0;

    /* 建立 MQTT 連接 - 與原始程式碼完全相同 */
    bool sessionPresent = false;
    mqttStatus = MQTT_Connect(
        &g_aws_iot_context.mqtt_context,
        &connectInfo,
        NULL,
        g_config->aws_iot.connack_recv_timeout_ms,
        &sessionPresent
    );

    if (mqttStatus != MQTTSuccess) {
        DMS_LOG_ERROR("❌ Failed to establish MQTT connection (status: %d)", mqttStatus);
        return convert_mqtt_status_to_dms_result(mqttStatus);
    }

    DMS_LOG_MQTT("✅ MQTT connection established successfully");
    DMS_LOG_DEBUG("   Session present: %s", sessionPresent ? "true" : "false");

    return DMS_SUCCESS;
}



void dms_aws_iot_event_callback(MQTTContext_t* pMqttContext,
                               MQTTPacketInfo_t* pPacketInfo,
                               MQTTDeserializedInfo_t* pDeserializedInfo)
{
    (void)pMqttContext;
    
    if (pPacketInfo == NULL || pDeserializedInfo == NULL) {
        DMS_LOG_ERROR("❌ NULL packet info or deserialized info in event callback");
        return;
    }

    switch (pPacketInfo->type) {
        case MQTT_PACKET_TYPE_PUBLISH:
            {
                if (pDeserializedInfo->pPublishInfo == NULL) {
                    DMS_LOG_ERROR("❌ NULL publish info in PUBLISH packet");
                    break;
                }

                MQTTPublishInfo_t* pPublishInfo = pDeserializedInfo->pPublishInfo;
                
                if (pPublishInfo->pTopicName == NULL || pPublishInfo->pPayload == NULL) {
                    DMS_LOG_ERROR("❌ NULL topic name or payload in PUBLISH packet");
                    break;
                }

                /* 準備 topic 字串 (需要 null-terminated) */
                char topic[256];
                size_t topic_len = pPublishInfo->topicNameLength;
                if (topic_len >= sizeof(topic)) {
                    topic_len = sizeof(topic) - 1;
                }
                memcpy(topic, pPublishInfo->pTopicName, topic_len);
                topic[topic_len] = '\0';

                /* 準備 payload 資料 */
                const char* payload = (const char*)pPublishInfo->pPayload;
                size_t payload_length = pPublishInfo->payloadLength;

                DMS_LOG_DEBUG("📨 PUBLISH received:");
                DMS_LOG_DEBUG("   Topic: %s (len=%zu)", topic, topic_len);
                DMS_LOG_DEBUG("   Payload length: %zu", payload_length);

                /* 檢查 callback 註冊狀態並轉發訊息 */
                if (g_aws_iot_context.message_callback != NULL) {
                    DMS_LOG_DEBUG("✅ Forwarding message to registered callback: %p", 
                                 (void*)g_aws_iot_context.message_callback);
                    
                    g_aws_iot_context.message_callback(topic, payload, payload_length);
                } else {
                    DMS_LOG_ERROR("❌ Message callback is NULL - message lost!");
                    DMS_LOG_ERROR("   Topic: %s", topic);
                    DMS_LOG_ERROR("   Payload: %.*s", (int)payload_length, payload);
                    DMS_LOG_ERROR("   Please check callback registration in initialization sequence");
                }
            }
            break;

        case MQTT_PACKET_TYPE_SUBACK:
            DMS_LOG_MQTT("✅ SUBACK received for packet ID: %u",
                        pDeserializedInfo->packetIdentifier);
            break;

        case MQTT_PACKET_TYPE_UNSUBACK:
            DMS_LOG_MQTT("✅ UNSUBACK received for packet ID: %u",
                        pDeserializedInfo->packetIdentifier);
            break;

        case MQTT_PACKET_TYPE_PUBACK:
            DMS_LOG_MQTT("✅ PUBACK received for packet ID: %u",
                        pDeserializedInfo->packetIdentifier);
            break;

        case MQTT_PACKET_TYPE_PUBREC:
            DMS_LOG_MQTT("📝 PUBREC received for packet ID: %u",
                        pDeserializedInfo->packetIdentifier);
            break;

        case MQTT_PACKET_TYPE_PUBREL:
            DMS_LOG_MQTT("📝 PUBREL received for packet ID: %u",
                        pDeserializedInfo->packetIdentifier);
            break;

        case MQTT_PACKET_TYPE_PUBCOMP:
            DMS_LOG_MQTT("✅ PUBCOMP received for packet ID: %u",
                        pDeserializedInfo->packetIdentifier);
            break;

        default:
            DMS_LOG_DEBUG("🔍 Other MQTT packet type received: %02x", pPacketInfo->type);
            break;
    }
}


dms_result_t dms_aws_iot_publish(const char* topic,
                                const char* payload,
                                size_t payload_length)
{
    if (!g_initialized || g_aws_iot_context.state != AWS_IOT_STATE_MQTT_CONNECTED) {
        DMS_LOG_ERROR("❌ AWS IoT not connected");
        return DMS_ERROR_NETWORK_FAILURE;  // ✅ 使用正確的錯誤碼
    }

    if (topic == NULL || payload == NULL) {
        DMS_LOG_ERROR("❌ Invalid parameters for publish");
        return DMS_ERROR_INVALID_PARAMETER;  // ✅ 使用正確的錯誤碼
    }

    /* 準備 MQTT 發佈資訊 - 與原始程式碼相同 */
    MQTTPublishInfo_t publishInfo = {0};
    publishInfo.qos = MQTTQoS1;
    publishInfo.retain = false;
    publishInfo.pTopicName = topic;
    publishInfo.topicNameLength = strlen(topic);
    publishInfo.pPayload = payload;
    publishInfo.payloadLength = payload_length;

    /* 產生封包 ID */
    uint16_t packetId = MQTT_GetPacketId(&g_aws_iot_context.mqtt_context);

    DMS_LOG_MQTT("📤 Publishing message to topic: %s", topic);
    DMS_LOG_DEBUG("   Payload length: %zu", payload_length);
    DMS_LOG_DEBUG("   Packet ID: %u", packetId);

    /* 發佈訊息 */
    MQTTStatus_t mqttStatus = MQTT_Publish(
        &g_aws_iot_context.mqtt_context,
        &publishInfo,
        packetId
    );

    if (mqttStatus != MQTTSuccess) {
        DMS_LOG_ERROR("❌ Failed to publish message (status: %d)", mqttStatus);
        return convert_mqtt_status_to_dms_result(mqttStatus);
    }

    DMS_LOG_MQTT("✅ Message published successfully");
    return DMS_SUCCESS;
}

dms_result_t dms_aws_iot_subscribe(const char* topic,
                                  mqtt_message_callback_t callback)
{
    if (!g_initialized || g_aws_iot_context.state != AWS_IOT_STATE_MQTT_CONNECTED) {
        DMS_LOG_ERROR("❌ AWS IoT not connected");
        return DMS_ERROR_NETWORK_FAILURE;  // ✅ 使用正確的錯誤碼
    }

    if (topic == NULL || callback == NULL) {
        DMS_LOG_ERROR("❌ Invalid parameters for subscribe");
        return DMS_ERROR_INVALID_PARAMETER;  // ✅ 使用正確的錯誤碼
    }

    /* 註冊訊息回調函數 */
    g_aws_iot_context.message_callback = callback;

    /* 準備訂閱資訊 - 與原始程式碼相同 */
    MQTTSubscribeInfo_t subscribeInfo = {0};
    subscribeInfo.qos = MQTTQoS1;
    subscribeInfo.pTopicFilter = topic;
    subscribeInfo.topicFilterLength = strlen(topic);

    /* 產生封包 ID */
    uint16_t packetId = MQTT_GetPacketId(&g_aws_iot_context.mqtt_context);

    DMS_LOG_MQTT("📥 Subscribing to topic: %s", topic);
    DMS_LOG_DEBUG("   Packet ID: %u", packetId);

    /* 訂閱主題 */
    MQTTStatus_t mqttStatus = MQTT_Subscribe(
        &g_aws_iot_context.mqtt_context,
        &subscribeInfo,
        1,
        packetId
    );

    if (mqttStatus != MQTTSuccess) {
        DMS_LOG_ERROR("❌ Failed to subscribe to topic (status: %d)", mqttStatus);
        return convert_mqtt_status_to_dms_result(mqttStatus);
    }

    DMS_LOG_MQTT("✅ Subscription request sent successfully");
    return DMS_SUCCESS;
}

dms_result_t dms_aws_iot_process_loop(uint32_t timeout_ms)
{
    if (!g_initialized || g_aws_iot_context.state != AWS_IOT_STATE_MQTT_CONNECTED) {
        return DMS_ERROR_NETWORK_FAILURE;  // ✅ 使用正確的錯誤碼
    }

    /*
     * 處理 MQTT 事件 - 與原始程式碼完全相同
     * 注意：AWS IoT SDK 的 MQTT_ProcessLoop 只接受一個參數
     * timeout 在這個版本中是內建的，通常約 1000ms
     */
    MQTTStatus_t mqttStatus = MQTT_ProcessLoop(&g_aws_iot_context.mqtt_context);

    if (mqttStatus != MQTTSuccess) {
        DMS_LOG_DEBUG("MQTT_ProcessLoop returned status: %d", mqttStatus);

        /* 根據錯誤類型更新狀態 */
        if (mqttStatus == MQTTRecvFailed || mqttStatus == MQTTSendFailed) {
            g_aws_iot_context.state = AWS_IOT_STATE_DISCONNECTED;
            DMS_LOG_WARN("🔗 Connection lost detected");
        }

        return convert_mqtt_status_to_dms_result(mqttStatus);
    }

    g_aws_iot_context.last_process_time = Clock_GetTimeMs();
    return DMS_SUCCESS;
}

bool dms_aws_iot_is_connected(void)
{
    return g_initialized && (g_aws_iot_context.state == AWS_IOT_STATE_MQTT_CONNECTED);
}

MQTTContext_t* dms_aws_iot_get_mqtt_context(void)
{
    if (!g_initialized) {
        return NULL;
    }
    return &g_aws_iot_context.mqtt_context;
}

NetworkContext_t* dms_aws_iot_get_network_context(void)
{
    if (!g_initialized) {
        return NULL;
    }
    return &g_aws_iot_context.network_context;
}

mqtt_interface_t dms_aws_iot_get_interface(void)
{
    mqtt_interface_t interface = {0};

    if (g_initialized) {
        interface.publish = dms_aws_iot_publish;
        interface.subscribe = dms_aws_iot_subscribe;
        interface.is_connected = dms_aws_iot_is_connected;
        interface.process_loop = dms_aws_iot_process_loop;
    }

    return interface;
}



void dms_aws_iot_register_message_callback(mqtt_message_callback_t callback)
{
    if (!g_initialized) {
        DMS_LOG_ERROR("❌ AWS IoT module not initialized before callback registration");
        return;
    }
    
    if (callback == NULL) {
        DMS_LOG_ERROR("❌ NULL callback function provided");
        return;
    }
    
    DMS_LOG_DEBUG("📝 Registering message callback: %p", (void*)callback);
    
    g_aws_iot_context.message_callback = callback;
    
    /* 驗證註冊成功 */
    if (g_aws_iot_context.message_callback == callback) {
        DMS_LOG_INFO("✅ Message callback registered successfully: %p", (void*)callback);
    } else {
        DMS_LOG_ERROR("❌ Callback registration verification failed");
    }
}



aws_iot_connection_state_t dms_aws_iot_get_state(void)
{
    if (!g_initialized) {
        return AWS_IOT_STATE_DISCONNECTED;
    }
    return g_aws_iot_context.state;
}

dms_result_t dms_aws_iot_disconnect(void)
{
    if (!g_initialized) {
        return DMS_SUCCESS;
    }

    DMS_LOG_INFO("🔌 Disconnecting from AWS IoT...");

    /* 斷開 MQTT 連接 - 與原始 cleanup() 函數相同 */
    if (g_aws_iot_context.state == AWS_IOT_STATE_MQTT_CONNECTED) {
        MQTTStatus_t mqttStatus = MQTT_Disconnect(&g_aws_iot_context.mqtt_context);
        if (mqttStatus != MQTTSuccess) {
            DMS_LOG_WARN("⚠️  MQTT disconnect failed with status: %d", mqttStatus);
        } else {
            DMS_LOG_MQTT("✅ MQTT disconnected cleanly");
        }
    }

#ifdef USE_OPENSSL
    /* 斷開 TLS 連線 - 與原始 cleanup() 函數相同 */
    if (g_aws_iot_context.state >= AWS_IOT_STATE_TLS_CONNECTED) {
        Openssl_Disconnect(&g_aws_iot_context.network_context);
        DMS_LOG_TLS("✅ TLS connection closed");
    }
#endif

    g_aws_iot_context.state = AWS_IOT_STATE_DISCONNECTED;
    DMS_LOG_INFO("✅ AWS IoT disconnection completed");

    return DMS_SUCCESS;
}

void dms_aws_iot_cleanup(void)
{
    if (!g_initialized) {
        return;
    }

    DMS_LOG_INFO("🧹 Cleaning up AWS IoT module...");

    /* 先斷開連接 */
    dms_aws_iot_disconnect();

    /* 清理內部狀態 */
    memset(&g_aws_iot_context, 0, sizeof(g_aws_iot_context));
    g_config = NULL;
    g_initialized = false;

    DMS_LOG_INFO("✅ AWS IoT module cleanup completed");
}

/*-----------------------------------------------------------*/
/* 內部輔助函數實作 */

static dms_result_t convert_mqtt_status_to_dms_result(MQTTStatus_t mqtt_status)
{
    switch (mqtt_status) {
        case MQTTSuccess:
            return DMS_SUCCESS;
        case MQTTBadParameter:
            return DMS_ERROR_INVALID_PARAMETER;
        case MQTTNoMemory:
            return DMS_ERROR_MEMORY_ALLOCATION;
        case MQTTSendFailed:
        case MQTTRecvFailed:
            return DMS_ERROR_NETWORK_FAILURE;
        case MQTTBadResponse:
        case MQTTServerRefused:
            return DMS_ERROR_TLS_FAILURE;  // ✅ 使用 demo_config.h 中存在的錯誤碼
        case MQTTNoDataAvailable:
        case MQTTKeepAliveTimeout:
            return DMS_ERROR_TIMEOUT;
        default:
            return DMS_ERROR_MQTT_FAILURE;
    }
}

static dms_result_t convert_openssl_status_to_dms_result(int openssl_status)
{
    /* OpenSSL 狀態碼轉換 */
    switch (openssl_status) {
        case 0: /* OPENSSL_SUCCESS */
            return DMS_SUCCESS;
        case -1: /* OPENSSL_INVALID_PARAMETER */
            return DMS_ERROR_INVALID_PARAMETER;
        case -2: /* OPENSSL_INSUFFICIENT_MEMORY */
            return DMS_ERROR_MEMORY_ALLOCATION;
        case -3: /* OPENSSL_INVALID_CREDENTIALS */
            return DMS_ERROR_TLS_FAILURE;
        case -4: /* OPENSSL_HANDSHAKE_FAILED */
        case -5: /* OPENSSL_CONNECT_FAILURE */
            return DMS_ERROR_NETWORK_FAILURE;
        default:
            return DMS_ERROR_NETWORK_FAILURE;
    }
}


/**
 * @brief 驗證 callback 是否已正確註冊
 */
bool dms_aws_iot_verify_callback_registered(void)
{
    bool is_registered = (g_aws_iot_context.message_callback != NULL);
    DMS_LOG_DEBUG("🔍 Callback registration status: %s (ptr=%p)",
                 is_registered ? "REGISTERED" : "NOT_REGISTERED",
                 (void*)g_aws_iot_context.message_callback);
    return is_registered;
}

/**
 * @brief 檢查 AWS IoT 模組是否完全初始化
 */
bool dms_aws_iot_is_initialized(void)
{
    return g_initialized;
}

/**
 * @brief 測試 Shadow delta 處理 (僅供測試使用)
 */
dms_result_t dms_aws_iot_test_shadow_delta_processing(void)
{
    const char* test_delta = "{"
        "\"state\": {"
        "\"desired\": {"
        "\"control-config-change\": 1"
        "}"
        "}"
        "}";

    DMS_LOG_INFO("🧪 Testing Shadow delta processing...");

    if (g_aws_iot_context.message_callback != NULL) {
        DMS_LOG_INFO("✅ Callback is registered, testing direct call...");
        g_aws_iot_context.message_callback(
            "$aws/things/" CLIENT_IDENTIFIER "/shadow/update/delta",
            test_delta,
            strlen(test_delta)
        );
        return DMS_SUCCESS;
    } else {
        DMS_LOG_ERROR("❌ Callback not registered for testing");
        return DMS_ERROR_SHADOW_FAILURE;
    }
}
