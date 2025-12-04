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

$(eval $(call BuildPackage,iptables))

romfs:
	$(INSTALL_DIR) $(ROMFSDIR)/usr/lib/xtables
	$(INSTALL_DIR) $(ROMFSDIR)/bin
	# 安装扩展库
	@if [ -f $(PKG_BUILD_DIR)/extensions/libxt_sni.so ]; then \
		$(INSTALL_BIN) $(PKG_BUILD_DIR)/extensions/libxt_sni.so $(ROMFSDIR)/usr/lib/xtables/; \
	fi
	@if [ -f $(PKG_BUILD_DIR)/extensions/libxt_webstr.so ]; then \
		$(INSTALL_BIN) $(PKG_BUILD_DIR)/extensions/libxt_webstr.so $(ROMFSDIR)/usr/lib/xtables/; \
	fi
	# 安装 iptables 二进制文件
	@if [ -f $(PKG_BUILD_DIR)/iptables/xtables-legacy-multi ]; then \
		$(INSTALL_BIN) $(PKG_BUILD_DIR)/iptables/xtables-legacy-multi $(ROMFSDIR)/bin/xtables-multi; \
		$(LN) xtables-multi $(ROMFSDIR)/bin/iptables; \
		$(LN) xtables-multi $(ROMFSDIR)/bin/iptables-restore; \
		$(LN) xtables-multi $(ROMFSDIR)/bin/iptables-save; \
	endif
ifdef CONFIG_IPV6
	@if [ -f $(PKG_BUILD_DIR)/iptables/xtables-legacy-multi ]; then \
		$(LN) xtables-multi $(ROMFSDIR)/bin/ip6tables; \
		$(LN) xtables-multi $(ROMFSDIR)/bin/ip6tables-restore; \
		$(LN) xtables-multi $(ROMFSDIR)/bin/ip6tables-save; \
	fi
endif