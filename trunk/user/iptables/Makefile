include $(ROOTDIR)/rules.mk

PKG_NAME:=iptables
PKG_VERSION:=1.8.7

PKG_FIXUP:=autoreconf
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk

SRC_DIR=./iptables-1.8.7

CONFIGURE_ARGS += \
	--enable-static \
	--enable-shared \
	--disable-nftables \
	--disable-connlabel \
	--with-xtlibdir=/usr/lib/xtables \
	--with-kernel=$(ROOTDIR)/$(LINUXDIR) \
	--with-xt-lock-name="/var/lock/xtables.lock" \
	--with-pic

# 修复扩展库编译选项
define Fix/Extensions
	@echo "修复扩展库编译选项..."
	@cd $(PKG_BUILD_DIR)/extensions && \
		sed -i 's/CFLAGS = /CFLAGS = -fPIC /g' Makefile && \
		sed -i 's/CPPFLAGS = /CPPFLAGS = -fPIC /g' Makefile
endef

Hooks/Compile/Pre += Fix/Extensions

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
