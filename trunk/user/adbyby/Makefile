THISDIR = $(shell pwd)
all:

clean:

romfs:
	# 直接创建所有需要的目录
	mkdir -p $(ROMFSDIR)/etc_ro 2>/dev/null || true
	mkdir -p $(ROMFSDIR)/usr/share/adbyby 2>/dev/null || true
	mkdir -p $(ROMFSDIR)/usr/share/adbyby/data 2>/dev/null || true
	mkdir -p $(ROMFSDIR)/usr/share/adbyby/doc 2>/dev/null || true
	mkdir -p $(ROMFSDIR)/usr/bin 2>/dev/null || true

	# 复制主程序文件（可执行文件）
	$(ROMFSINST) -p +x "$(THISDIR)/share/adbyby" /usr/share/adbyby/ || true

	# 复制配置文件
	$(ROMFSINST) "$(THISDIR)/share/adblack.conf" /usr/share/adbyby/ || true
	$(ROMFSINST) "$(THISDIR)/share/adesc.conf" /usr/share/adbyby/ || true
	$(ROMFSINST) "$(THISDIR)/share/adhook.ini" /usr/share/adbyby/ || true
	$(ROMFSINST) "$(THISDIR)/share/adhost.conf" /usr/share/adbyby/ || true
	$(ROMFSINST) "$(THISDIR)/share/blockip.conf" /usr/share/adbyby/ || true
	$(ROMFSINST) "$(THISDIR)/share/rules.txt" /usr/share/adbyby/ || true
	$(ROMFSINST) "$(THISDIR)/share/update.info" /usr/share/adbyby/ || true
	$(ROMFSINST) "$(THISDIR)/share/user.action" /usr/share/adbyby/ || true
	$(ROMFSINST) "$(THISDIR)/share/dnsmasq.adblock" /usr/share/adbyby/ || true
	$(ROMFSINST) "$(THISDIR)/share/dnsmasq.ads" /usr/share/adbyby/ || true
	$(ROMFSINST) "$(THISDIR)/share/dnsmasq.esc" /usr/share/adbyby/ || true
	$(ROMFSINST) "$(THISDIR)/share/firewall.include" /usr/share/adbyby/ || true

	# 复制脚本文件（带执行权限）
	$(ROMFSINST) -p +x "$(THISDIR)/share/adblock.sh" /usr/share/adbyby/ || true
	$(ROMFSINST) -p +x "$(THISDIR)/share/adbyby.sh" /usr/share/adbyby/ || true
	$(ROMFSINST) -p +x "$(THISDIR)/share/adbybyfirst.sh" /usr/share/adbyby/ || true
	$(ROMFSINST) -p +x "$(THISDIR)/share/adbybyupdate.sh" /usr/share/adbyby/ || true
	$(ROMFSINST) -p +x "$(THISDIR)/share/admem.sh" /usr/share/adbyby/ || true
	$(ROMFSINST) -p +x "$(THISDIR)/share/adupdate.sh" /usr/share/adbyby/ || true

	# 复制data目录文件
	$(ROMFSINST) "$(THISDIR)/share/data/lazy.txt" /usr/share/adbyby/data/ || true
	$(ROMFSINST) "$(THISDIR)/share/data/rules.txt" /usr/share/adbyby/data/ || true
	$(ROMFSINST) "$(THISDIR)/share/data/user.txt" /usr/share/adbyby/data/ || true
	$(ROMFSINST) "$(THISDIR)/share/data/video.txt" /usr/share/adbyby/data/ || true

	# 复制doc目录文件
	$(ROMFSINST) "$(THISDIR)/share/doc/hidecss.js" /usr/share/adbyby/doc/ || true

	# 自定义配置脚本
	$(ROMFSINST) /etc_ro/adbyby_rules.sh
	$(ROMFSINST) /etc_ro/adbyby_adblack.sh
	$(ROMFSINST) /etc_ro/adbyby_adesc.sh
	$(ROMFSINST) /etc_ro/adbyby_adhost.sh
	$(ROMFSINST) /etc_ro/adbyby_host.sh
	$(ROMFSINST) /etc_ro/adbyby_blockip.sh
	# 复制主脚本到/usr/bin
	$(ROMFSINST) -p +x $(THISDIR)/adbyby.sh /usr/bin/adbyby.sh