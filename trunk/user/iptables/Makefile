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

# 确保自定义扩展库被编译
define Build/Compile/Extensions
	$(MAKE_VARS) \
	$(MAKE) $(PKG_JOBS) -C $(PKG_BUILD_DIR)/extensions \
		$(MAKE_FLAGS) \
		libxt_sni.so libxt_webstr.so
endef

# 修复运行时环境问题
define Fix/Environment
	@echo "清理编译环境变量..."
	@sed -i 's|/home/runner/work/padavan/padavan/trunk|/opt|g' $(PKG_BUILD_DIR)/iptables/iptables.c
	@sed -i 's|ccache||g' $(PKG_BUILD_DIR)/iptables/iptables.c
	@sed -i 's|/usr/bin/sed|/bin/sed|g' $(PKG_BUILD_DIR)/iptables/iptables.c
endef

Hooks/Compile/Post += Build/Compile/Extensions
Hooks/PreCompile += Fix/Environment

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
	# 添加以下两行，确保库文件被安装到固件中
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
# 修复运行时环境问题
define Fix/Environment
	@echo "清理编译环境变量..."
	@sed -i 's|/home/runner/work/padavan/padavan/trunk|/opt|g' $(PKG_BUILD_DIR)/iptables/iptables.c
	@sed -i 's|ccache||g' $(PKG_BUILD_DIR)/iptables/iptables.c
endef

Hooks/PreCompile += Fix/Environment
