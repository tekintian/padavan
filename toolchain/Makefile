TOPDIR        = ${CURDIR}
CT_VERSION    := 1.26.0
CT_DIR        := $(TOPDIR)/crosstool-ng-$(CT_VERSION)
CT_PREFIX     := $(TOPDIR)/toolchain-mipsel
CT_TARGET     := mipsel-linux-musl
CT_URL        := https://github.com/crosstool-ng/crosstool-ng/releases/download/crosstool-ng-$(CT_VERSION)/crosstool-ng-$(CT_VERSION).tar.xz
TOOLCHAIN_URL := https://github.com/tekintian/padavan/releases/download/toolchain/$(CT_TARGET).tar.xz
CT_LOCAL_FILE := $(TOPDIR)/files/crosstool-ng-$(CT_VERSION).tar.xz

# 使用环境变量中的bash路径，默认使用/bin/bash
BASH := $(or $(BASH),/bin/bash)

all: build

build:
	@if [ ! -d $(CT_DIR) ]; then \
		echo "crosstool-ng-$(CT_VERSION) not found, installing..."; \
		if [ -f $(CT_LOCAL_FILE) ]; then \
			echo "Found local crosstool-ng-$(CT_VERSION).tar.xz, extracting..."; \
			tar -Jxf $(CT_LOCAL_FILE); \
		else \
			echo "Downloading crosstool-ng-$(CT_VERSION)..."; \
			curl -fSsLo- $(CT_URL) | tar Jx; \
		fi; \
	fi

	@echo "Building toolchain..."
	cp -r $(TOPDIR)/configs/$(CT_TARGET) $(CT_DIR)/samples
	@(cd $(CT_DIR); \
		$(BASH) ./bootstrap && \
		$(BASH) ./configure --enable-local && \
		make && \
		./ct-ng $(CT_TARGET) && \
		./ct-ng build \
	)

clean:
	@if [ -d $(CT_DIR) ]; then \
		cd $(CT_DIR); \
		if [ -f ct-ng ]; then ./ct-ng distclean; fi; \
		if [ -f Makefile ]; then make distclean; fi; \
	fi
	@if [ -d $(CT_PREFIX) ]; then rm -rf $(CT_PREFIX); fi

download:
	@if [ ! -d $(CT_PREFIX) ]; then \
		echo "Downloading toolchain..."; \
		mkdir -p $(CT_PREFIX); \
		curl -fSsLo- $(TOOLCHAIN_URL) | tar Jx -C $(CT_PREFIX); \
	fi
ifeq ($(CT_TARGET),mipsel-linux-musl)
	@if [ ! -f $(CT_PREFIX)/$(CT_TARGET)/sysroot/usr/include/sys/queue.h ]; then \
		echo "Installing sys/queue.h..."; \
		if [ -f $(TOPDIR)/files/musl/sys/queue.h ]; then \
			cp $(TOPDIR)/files/musl/sys/queue.h $(CT_PREFIX)/$(CT_TARGET)/sysroot/usr/include/sys/queue.h; \
			echo "Successfully copied local queue.h"; \
		elif curl -fSsL -o $(CT_PREFIX)/$(CT_TARGET)/sysroot/usr/include/sys/queue.h \
			"https://github.com/tekintian/padavan/raw/refs/heads/main/toolchain/files/musl/sys/queue.h" ; then \
			echo "Successfully downloaded queue.h from GitHub"; \
		else \
			echo "Warning: Failed to get queue.h, but continuing..."; \
			echo "You may need to manually install sys/queue.h if compilation fails"; \
		fi; \
	else \
		echo "sys/queue.h already exists, skipping copy"; \
	fi
endif