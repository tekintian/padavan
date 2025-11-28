THISDIR = $(shell pwd)
all:

clean:

romfs:
	# 1. 复制etc_ro目录下的所有脚本文件
	$(ROMFSINST) -p +x $(THISDIR)/etc_ro/adbyby_adblack.sh /etc_ro/adbyby_adblack.sh
	$(ROMFSINST) -p +x $(THISDIR)/etc_ro/adbyby_adesc.sh /etc_ro/adbyby_adesc.sh
	$(ROMFSINST) -p +x $(THISDIR)/etc_ro/adbyby_adhost.sh /etc_ro/adbyby_adhost.sh
	$(ROMFSINST) -p +x $(THISDIR)/etc_ro/adbyby_blockip.sh /etc_ro/adbyby_blockip.sh
	$(ROMFSINST) -p +x $(THISDIR)/etc_ro/adbyby_host.sh /etc_ro/adbyby_host.sh
	$(ROMFSINST) -p +x $(THISDIR)/etc_ro/adbyby_rules.sh /etc_ro/adbyby_rules.sh
	
	# 2. 创建目录结构
	$(ROMFSINST) -d /usr/share/adbyby
	$(ROMFSINST) -d /usr/share/adbyby/data
	$(ROMFSINST) -d /usr/share/adbyby/doc
	
	# 3. 复制配置文件
	$(ROMFSINST) $(THISDIR)/share/adblack.conf /usr/share/adbyby/adblack.conf
	$(ROMFSINST) $(THISDIR)/share/adesc.conf /usr/share/adbyby/adesc.conf
	$(ROMFSINST) $(THISDIR)/share/adhook.ini /usr/share/adbyby/adhook.ini
	$(ROMFSINST) $(THISDIR)/share/adhost.conf /usr/share/adbyby/adhost.conf
	$(ROMFSINST) $(THISDIR)/share/blockip.conf /usr/share/adbyby/blockip.conf
	$(ROMFSINST) $(THISDIR)/share/rules.txt /usr/share/adbyby/rules.txt
	$(ROMFSINST) $(THISDIR)/share/update.info /usr/share/adbyby/update.info
	$(ROMFSINST) $(THISDIR)/share/user.action /usr/share/adbyby/user.action
	$(ROMFSINST) $(THISDIR)/share/dnsmasq.adblock /usr/share/adbyby/dnsmasq.adblock
	$(ROMFSINST) $(THISDIR)/share/dnsmasq.ads /usr/share/adbyby/dnsmasq.ads
	$(ROMFSINST) $(THISDIR)/share/dnsmasq.esc /usr/share/adbyby/dnsmasq.esc
	$(ROMFSINST) $(THISDIR)/share/firewall.include /usr/share/adbyby/firewall.include
	
	# 4. 复制可执行文件和脚本
	if [ -f "$(THISDIR)/share/adbyby" ]; then \
	  $(ROMFSINST) -p +x $(THISDIR)/share/adbyby /usr/share/adbyby/adbyby; \
	fi
	$(ROMFSINST) -p +x $(THISDIR)/share/adblock.sh /usr/share/adbyby/adblock.sh
	$(ROMFSINST) -p +x $(THISDIR)/share/adbyby.sh /usr/share/adbyby/adbyby.sh
	$(ROMFSINST) -p +x $(THISDIR)/share/adbybyfirst.sh /usr/share/adbyby/adbybyfirst.sh
	$(ROMFSINST) -p +x $(THISDIR)/share/adbybyupdate.sh /usr/share/adbyby/adbybyupdate.sh
	$(ROMFSINST) -p +x $(THISDIR)/share/admem.sh /usr/share/adbyby/admem.sh
	$(ROMFSINST) -p +x $(THISDIR)/share/adupdate.sh /usr/share/adbyby/adupdate.sh

	# 5. 复制data目录内容
	$(ROMFSINST) $(THISDIR)/share/data/lazy.txt /usr/share/adbyby/data/lazy.txt
	$(ROMFSINST) $(THISDIR)/share/data/rules.txt /usr/share/adbyby/data/rules.txt
	$(ROMFSINST) $(THISDIR)/share/data/user.txt /usr/share/adbyby/data/user.txt
	$(ROMFSINST) $(THISDIR)/share/data/video.txt /usr/share/adbyby/data/video.txt
	
	# 6. 复制doc目录内容
	$(ROMFSINST) $(THISDIR)/share/doc/hidecss.js /usr/share/adbyby/doc/hidecss.js
	
	# 7. 主脚本安装
	$(ROMFSINST) -p +x $(THISDIR)/adbyby.sh /usr/bin/adbyby.sh