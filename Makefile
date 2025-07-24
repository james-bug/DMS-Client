


include $(TOPDIR)/rules.mk

PKG_NAME:=dms-client
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_LICENSE:=MIT
PKG_BUILD_PARALLEL:=1
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

# AWS IoT SDK 路徑 (保持不變)
AWS_IOT_SDK_DIR:=$(BUILD_DIR)/aws-iot-device-sdk-embedded-C-202412.00

# BCML 路徑 (完全模仿 AWS IoT 方式)
BCML_DIR:=$(BUILD_DIR)/bcml-1.0.0

define Package/dms-client
        SECTION:=BenQ
        CATEGORY:=BenQ
        TITLE:=DMS Client with AWS IoT SDK
        SUBMENU:=Applications
        DEPENDS:=+libopenssl +libcurl +libpthread +librt +cJSON +libuci  
endef

define Package/dms-client/description
        DMS Client integrated with AWS IoT Device SDK for Embedded C
        using MQTT over TLS with mutual authentication.
        Includes BCML Middleware for WiFi control.
endef

# CMake 選項 (完全模仿 AWS IoT 方式)
CMAKE_OPTIONS += \
       -DAWS_IOT_SDK_ROOT=$(AWS_IOT_SDK_DIR) \
       -DBCML_ROOT=$(BCML_DIR)

# 準備階段：只複製源碼檔案 (保持不變)
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)/src
	mkdir -p $(PKG_BUILD_DIR)/certificates
	$(CP) ./src/* $(PKG_BUILD_DIR)/src/
	$(CP) ./certificates/* $(PKG_BUILD_DIR)/certificates/ 2>/dev/null || true
	$(CP) ./CMakeLists.txt $(PKG_BUILD_DIR)
endef

# 配置階段：傳遞兩個 SDK 路徑 (修改這裡)
define Build/Configure
	$(call Build/Configure/Default,-DAWS_IOT_SDK_ROOT=$(AWS_IOT_SDK_DIR) -DBCML_ROOT=$(BCML_DIR))
endef

define Package/dms-client/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dms-client $(1)/usr/bin/

	$(INSTALL_DIR) $(1)/etc/dms-client
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/certificates/* $(1)/etc/dms-client/ 2>/dev/null || true

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/dms-client.init $(1)/etc/init.d/dms-client

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) ./files/dms-client.config $(1)/etc/config/dms-client
endef

$(eval $(call BuildPackage,dms-client))
