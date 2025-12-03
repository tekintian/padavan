include $(ROOTDIR)/rules.mk

PKG_NAME:=iptables
PKG_VERSION:=1.8.7

PKG_FIXUP:=autoreconf
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk

SRC_DIR=./iptables-1.8.7

CONFIGURE_ARGS += \
	--disable-static \
	--enable-shared \
	--disable-nftables \
	--disable-connlabel \
	--with-xtlibdir=/usr/lib/xtables \
	--with-kernel=$(ROOTDIR)/$(LINUXDIR) \
	--with-xt-lock-name="/var/lock/xtables.lock"

# 简化的编译流程
define Build/Compile
	$(call Build/Compile/Default)
	# 编译扩展库
	$(MAKE_VARS) \
	$(MAKE) $(PKG_JOBS) -C $(PKG_BUILD_DIR)/extensions \
		$(MAKE_FLAGS) \
		libxt_sni.so libxt_webstr.so
endef

after_install:
	$(INSTALL_DIR) $(ROMFSDIR)/usr/lib/xtables
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/extensions/libxt_sni.so $(ROMFSDIR)/usr/lib/xtables/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/extensions/libxt_webstr.so $(ROMFSDIR)/usr/lib/xtables/

$(eval $(call BuildPackage,iptables))

romfs:
	$(INSTALL_DIR) $(ROMFSDIR)/usr/lib/xtables
	$(INSTALL_DIR) $(ROMFSDIR)/bin
	# 确保扩展库已编译
	$(MAKE) -C $(PKG_BUILD_DIR)/extensions libxt_sni.so libxt_webstr.so
	# 安装扩展库和二进制文件
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/extensions/libxt_sni.so $(ROMFSDIR)/usr/lib/xtables/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/extensions/libxt_webstr.so $(ROMFSDIR)/usr/lib/xtables/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/iptables/xtables-legacy-multi $(ROMFSDIR)/bin/xtables-multi
	$(LN) xtables-multi $(ROMFSDIR)/bin/iptables
	$(LN) xtables-multi $(ROMFSDIR)/bin/iptables-restore
	$(LN) xtables-multi $(ROMFSDIR)/bin/iptables-save
ifdef CONFIG_IPV6
	$(LN) xtables-multi $(ROMFSDIR)/bin/ip6tables
	$(LN) xtables-multi $(ROMFSDIR)/bin/ip6tables-restore
	$(LN) xtables-multi $(ROMFSDIR)/bin/ip6tables-save
endif
