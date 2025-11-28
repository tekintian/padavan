THISDIR = $(shell pwd)
all:

clean:

romfs:
	# 先创建基础目录（如果需要）
	$(ROMFSINST) -d /etc_ro
	# 确保/usr/share目录存在
	mkdir -p $(ROMFSDIR)/usr/share 2>/dev/null || true
	# 创建adbyby目录
	$(ROMFSINST) -d /usr/share/adbyby
	# 创建子目录
	$(ROMFSINST) -d /usr/share/adbyby/data
	$(ROMFSINST) -d /usr/share/adbyby/doc

	# 复制etc_ro目录下的所有脚本文件
	for file in $(THISDIR)/etc_ro/*.sh; do \
		if [ -f "$$file" ]; then \
			$(ROMFSINST) -p +x "$$file" /etc_ro/; \
		fi \
	done

	# 复制share目录下的配置文件和脚本（排除子目录）
	for file in $(THISDIR)/share/*.conf $(THISDIR)/share/*.sh $(THISDIR)/share/*.ini $(THISDIR)/share/*.txt $(THISDIR)/share/*.info $(THISDIR)/share/*.action $(THISDIR)/share/firewall.include; do \
		if [ -f "$$file" ]; then \
			filename=$$(basename "$$file"); \
			if [ "$$filename" = "adbyby" ]; then \
				$(ROMFSINST) -p +x "$$file" /usr/share/adbyby/; \
			else \
				$(ROMFSINST) "$$file" /usr/share/adbyby/; \
			fi \
		fi \
	done

	# 复制data目录内容
	if [ -d "$(THISDIR)/share/data" ]; then \
		for file in $(THISDIR)/share/data/*.txt; do \
			if [ -f "$$file" ]; then \
				$(ROMFSINST) "$$file" /usr/share/adbyby/data/; \
			fi \
		done \
	fi

	# 复制doc目录内容
	if [ -d "$(THISDIR)/share/doc" ]; then \
		for file in $(THISDIR)/share/doc/*.js; do \
			if [ -f "$$file" ]; then \
				$(ROMFSINST) "$$file" /usr/share/adbyby/doc/; \
			fi \
		done \
	fi

	# 主脚本安装
	if [ -f "$(THISDIR)/adbyby.sh" ]; then \
		$(ROMFSINST) -p +x "$(THISDIR)/adbyby.sh" /usr/bin/; \
	fi