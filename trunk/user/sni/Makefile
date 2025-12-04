include $(ROOTDIR)/rules.mk

PKG_NAME:=sni
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk

$(eval $(call BuildPackage,sni))

romfs:
	$(INSTALL_DIR) $(ROMFSDIR)/usr/lib/xtables
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libxt_sni.so $(ROMFSDIR)/usr/lib/xtables/