

cmake_minimum_required(VERSION 3.13.0)
project(dms-client C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# AWS IoT SDK 路徑 - 透過 Makefile 參數傳遞 (保持完全不變)
if(NOT DEFINED AWS_IOT_SDK_ROOT)
    message(FATAL_ERROR "AWS_IOT_SDK_ROOT not defined. Please specify -DAWS_IOT_SDK_ROOT=<path>")
endif()

message(STATUS "Using AWS IoT SDK from: ${AWS_IOT_SDK_ROOT}")

# 檢查 SDK 路徑是否存在
if(NOT EXISTS ${AWS_IOT_SDK_ROOT})
    message(FATAL_ERROR "AWS IoT SDK path does not exist: ${AWS_IOT_SDK_ROOT}")
endif()

# === BCML 路徑配置 (完全模仿 AWS IoT 方式) ===
if(DEFINED BCML_ROOT)
    message(STATUS "Using BCML from: ${BCML_ROOT}")

    # 檢查 BCML 路徑是否存在
    if(EXISTS ${BCML_ROOT})
        message(STATUS "✅ BCML Middleware found at: ${BCML_ROOT}")
        set(BCML_ENABLED TRUE)
        add_definitions(-DBCML_MIDDLEWARE_ENABLED)
        add_definitions(-DUCI_API_ENABLE)

        # 尋找 BCML 靜態庫 (根據您提供的路徑結構)
        set(BCML_LIB_PATHS
            "${BCML_ROOT}/libbcml.a"
            "${BCML_ROOT}/ipkg-install/usr/lib/libbcml.a"
            "${BCML_ROOT}/.pkgdir/bcml/usr/lib/libbcml.a"
            "${BCML_ROOT}/ipkg-aarch64_cortex-a53/bcml/usr/lib/libbcml.a"
        )

        set(BCML_LIB "")
        foreach(LIB_PATH ${BCML_LIB_PATHS})
            if(EXISTS ${LIB_PATH})
                set(BCML_LIB ${LIB_PATH})
                message(STATUS "✅ BCML library found: ${BCML_LIB}")
                break()
            endif()
        endforeach()

        if(NOT BCML_LIB)
            message(WARNING "⚠️  BCML library not found, disabling BCML support")
            set(BCML_ENABLED FALSE)
        endif()
    else()
        message(WARNING "⚠️  BCML path does not exist: ${BCML_ROOT}, disabling BCML support")
        set(BCML_ENABLED FALSE)
    endif()
else()
    message(STATUS "BCML_ROOT not defined, compiling without BCML support")
    set(BCML_ENABLED FALSE)
endif()

# 包含目錄 (保持 AWS IoT 完全不變，條件性加入 BCML)
set(INCLUDE_DIRS
    ${AWS_IOT_SDK_ROOT}/source/include
    ${AWS_IOT_SDK_ROOT}/libraries/standard/coreMQTT/source/include
    ${AWS_IOT_SDK_ROOT}/libraries/standard/coreMQTT/source/interface
    ${AWS_IOT_SDK_ROOT}/libraries/standard/coreJSON/source/include
    ${AWS_IOT_SDK_ROOT}/platform/include
    ${AWS_IOT_SDK_ROOT}/platform/posix/transport/include
    ${AWS_IOT_SDK_ROOT}/platform/posix/clock/include
    ${AWS_IOT_SDK_ROOT}/demos/logging-stack
    ${AWS_IOT_SDK_ROOT}/demos/mqtt/mqtt_demo_mutual_auth
    src
)

# 如果 BCML 啟用，加入 BCML 包含目錄
if(BCML_ENABLED)
    list(APPEND INCLUDE_DIRS
        ${BCML_ROOT}/src/include
        ${BCML_ROOT}/src/lib/core
    )
endif()

include_directories(${INCLUDE_DIRS})

# AWS IoT SDK 源文件 - 根據 mqtt_demo_mutual_auth.c 參考修正路徑 (保持完全不變)
# 檢查並列出實際找到的文件
file(GLOB CORE_MQTT_SOURCES
    "${AWS_IOT_SDK_ROOT}/libraries/standard/coreMQTT/source/*.c"
)

file(GLOB CORE_JSON_SOURCES
    "${AWS_IOT_SDK_ROOT}/libraries/standard/coreJSON/source/*.c"
)

# 也嘗試備用路徑
if(NOT CORE_MQTT_SOURCES)
    file(GLOB CORE_MQTT_SOURCES
        "${AWS_IOT_SDK_ROOT}/source/coreMQTT/*.c"
        "${AWS_IOT_SDK_ROOT}/coreMQTT/source/*.c"
    )
endif()

if(NOT CORE_JSON_SOURCES)
    file(GLOB CORE_JSON_SOURCES
        "${AWS_IOT_SDK_ROOT}/source/coreJSON/*.c"
        "${AWS_IOT_SDK_ROOT}/coreJSON/source/*.c"
    )
endif()

# Transport 源文件 - 檢查多個可能路徑 (保持完全不變)
set(TRANSPORT_SOURCES)
foreach(TRANSPORT_PATH
    "${AWS_IOT_SDK_ROOT}/platform/posix/transport/src/openssl_posix.c"
    "${AWS_IOT_SDK_ROOT}/demos/network_manager/openssl_posix.c"
    "${AWS_IOT_SDK_ROOT}/source/openssl_posix.c"
)
    if(EXISTS ${TRANSPORT_PATH})
        list(APPEND TRANSPORT_SOURCES ${TRANSPORT_PATH})
        break()
    endif()
endforeach()

foreach(SOCKET_PATH
    "${AWS_IOT_SDK_ROOT}/platform/posix/transport/src/sockets_posix.c"
    "${AWS_IOT_SDK_ROOT}/demos/network_manager/sockets_posix.c"
    "${AWS_IOT_SDK_ROOT}/source/sockets_posix.c"
)
    if(EXISTS ${SOCKET_PATH})
        list(APPEND TRANSPORT_SOURCES ${SOCKET_PATH})
        break()
    endif()
endforeach()

# Clock 源文件 - 檢查多個可能路徑 (保持完全不變)
set(CLOCK_SOURCES)
foreach(CLOCK_PATH
    "${AWS_IOT_SDK_ROOT}/platform/posix/clock_posix.c"
    "${AWS_IOT_SDK_ROOT}/platform/posix/clock/clock_posix.c"
    "${AWS_IOT_SDK_ROOT}/demos/clock_posix.c"
    "${AWS_IOT_SDK_ROOT}/source/clock_posix.c"
)
    if(EXISTS ${CLOCK_PATH})
        list(APPEND CLOCK_SOURCES ${CLOCK_PATH})
        break()
    endif()
endforeach()

# 檢查所有必要的源文件是否找到 (保持完全不變)
if(NOT CORE_MQTT_SOURCES)
    message(FATAL_ERROR "❌ coreMQTT sources not found in AWS IoT SDK")
endif()

if(NOT CORE_JSON_SOURCES)
    message(FATAL_ERROR "❌ coreJSON sources not found in AWS IoT SDK")
endif()

if(NOT TRANSPORT_SOURCES)
    message(FATAL_ERROR "❌ Transport sources not found in AWS IoT SDK")
endif()

if(NOT CLOCK_SOURCES)
    message(FATAL_ERROR "❌ Clock sources not found in AWS IoT SDK")
endif()

# DMS Client 源文件 - 新增 DMS API 客戶端 (保持原有)
set(DMS_CLIENT_SOURCES
    src/dms_client.c
    src/dms_api_client.c
    src/dms_log.c
    src/dms_config.c
    src/dms_aws_iot.c
    src/dms_shadow.c
    src/dms_command.c
    src/dms_reconnect.c
)

# 如果 BCML 啟用，加入適配器
if(BCML_ENABLED)
    list(APPEND DMS_CLIENT_SOURCES src/bcml_adapter.c)
    message(STATUS "✅ BCML adapter included in build")
endif()

# 顯示找到的檔案數量 (保持原有)
list(LENGTH CORE_MQTT_SOURCES mqtt_count)
list(LENGTH CORE_JSON_SOURCES json_count)
list(LENGTH TRANSPORT_SOURCES transport_count)
list(LENGTH CLOCK_SOURCES clock_count)

message(STATUS "Found ${mqtt_count} MQTT source files")
message(STATUS "Found ${json_count} JSON source files")
message(STATUS "Found ${transport_count} transport source files")
message(STATUS "Found ${clock_count} clock source files")

# 創建執行檔 (保持原有)
add_executable(dms-client
    ${DMS_CLIENT_SOURCES}
    ${CORE_MQTT_SOURCES}
    ${CORE_JSON_SOURCES}
    ${TRANSPORT_SOURCES}
    ${CLOCK_SOURCES}
)

# 尋找並連結 OpenSSL 和 libcurl (保持原有)
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(CURL REQUIRED libcurl)

target_link_libraries(dms-client
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    pthread
    rt
)

# 如果 BCML 啟用，連結 BCML 相關庫
if(BCML_ENABLED)
    target_link_libraries(dms-client ${BCML_LIB})
    target_link_libraries(dms-client cjson)  # OpenWrt cJSON 套件
    target_link_libraries(dms-client uci)    # OpenWrt UCI 套件
    message(STATUS "🔗 BCML middleware libraries linked")
endif()

# 編譯定義 (保持原有 + 修正)
target_compile_definitions(dms-client PRIVATE
    USE_OPENSSL=1
    LOGGING_LEVEL_DEBUG=1
    DMS_API_ENABLED=1
    DMS_AES_ENABLED=1
)

# 安裝規則 (保持原有)
install(TARGETS dms-client DESTINATION bin)

# 顯示配置摘要
message(STATUS "=== DMS Client Configuration Summary ===")
message(STATUS "AWS IoT SDK: ${AWS_IOT_SDK_ROOT}")
if(BCML_ENABLED)
    message(STATUS "BCML Middleware: ENABLED (${BCML_ROOT})")
    message(STATUS "BCML Library: ${BCML_LIB}")
else()
    message(STATUS "BCML Middleware: DISABLED")
endif()
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "========================================")
