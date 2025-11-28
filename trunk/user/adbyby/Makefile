THISDIR = $(shell pwd)
all:

clean:

romfs:
	# 1. 循环安装etc_ro目录下的所有脚本文件到目标系统的/etc_ro/目录
	for file in $(THISDIR)/etc_ro/*.sh; do \
	  basename=$$(basename "$$file"); \
	  $(ROMFSINST) -p +x "$$file" "/etc_ro/$$basename"; \
	done
	
	# 2. 创建adbyby主目录结构
	$(ROMFSINST) -d /usr/share/adbyby
	$(ROMFSINST) -d /usr/share/adbyby/data
	$(ROMFSINST) -d /usr/share/adbyby/doc
	
	# 3. 复制share目录下的所有配置文件文件到目标系统的/usr/share/adbyby/目录
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
	
	# 可执行文件和脚本
	$(ROMFSINST) -p +x $(THISDIR)/share/adbyby /usr/share/adbyby/adbyby
	$(ROMFSINST) -p +x $(THISDIR)/share/adblock.sh /usr/share/adbyby/adblock.sh
	$(ROMFSINST) -p +x $(THISDIR)/share/adbyby.sh /usr/share/adbyby/adbyby.sh
	$(ROMFSINST) -p +x $(THISDIR)/share/adbybyfirst.sh /usr/share/adbyby/adbybyfirst.sh
	$(ROMFSINST) -p +x $(THISDIR)/share/adbybyupdate.sh /usr/share/adbyby/adbybyupdate.sh
	$(ROMFSINST) -p +x $(THISDIR)/share/admem.sh /usr/share/adbyby/admem.sh
	$(ROMFSINST) -p +x $(THISDIR)/share/adupdate.sh /usr/share/adbyby/adupdate.sh

	# data目录内容
	for file in $(THISDIR)/share/data/*.txt; do \
	  basename=$$(basename "$$file"); \
	  $(ROMFSINST) -p +x "$$file" "/usr/share/adbyby/data/$$basename"; \
	done
	
	# doc目录内容
	$(ROMFSINST) $(THISDIR)/share/doc/hidecss.js /usr/share/adbyby/doc/hidecss.js
	
	# 4. 主脚本安装
	$(ROMFSINST) -p +x $(THISDIR)/adbyby.sh /usr/bin/adbyby.sh