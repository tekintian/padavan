TOPDIR        = ${CURDIR}
CT_VERSION    := 1.26.0
CT_DIR        := $(TOPDIR)/crosstool-ng-$(CT_VERSION)
CT_PREFIX     := $(TOPDIR)/toolchain-mipsel
CT_TARGET     := mipsel-linux-musl
CT_URL        := https://github.com/crosstool-ng/crosstool-ng/releases/download/crosstool-ng-$(CT_VERSION)/crosstool-ng-$(CT_VERSION).tar.xz
TOOLCHAIN_URL := https://github.com/tekintian/padavan/releases/download/toolchain/$(CT_TARGET).tar.xz
CT_LOCAL_FILE := $(TOPDIR)/files/crosstool-ng-$(CT_VERSION).tar.xz

# uClibc-ng download URLs
UCLIBC_NG_LOCAL := $(TOPDIR)/files/uClibc-ng-1.0.43.tar.xz
UCLIBC_NG_OFFICIAL := https://downloads.uclibc-ng.org/releases/1.0.43/uClibc-ng-1.0.43.tar.xz

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
	@echo "Patching crosstool-NG.sh for macOS case-insensitive filesystem..."
	@sed -i.bak '122s/CT_TestAndAbort/CT_DoLog WARN/' $(CT_DIR)/scripts/crosstool-NG.sh
	@sed -i.bak '329s/CT_TestAndAbort/CT_DoLog WARN/' $(CT_DIR)/scripts/crosstool-NG.sh
	@rm -f $(CT_DIR)/scripts/crosstool-NG.sh.bak
	@echo "Updating config.guess and config.sub for modern system compatibility..."
	@for dir in $(CT_DIR) $(CT_DIR)/config; do \
		if [ -f $$dir/config.guess ]; then \
			curl -fsSL -o $$dir/config.guess 'https://git.savannah.gnu.org/cgit/config.git/plain/config.guess'; \
		fi; \
		if [ -f $$dir/config.sub ]; then \
			curl -fsSL -o $$dir/config.sub 'https://git.savannah.gnu.org/cgit/config.git/plain/config.sub'; \
		fi; \
	done
	@echo "Pre-downloading uClibc-ng source for reliability..."
	@mkdir -p $(CT_DIR)/.build/tarballs
	@if [ ! -f $(CT_DIR)/.build/tarballs/uClibc-ng-1.0.43.tar.xz ]; then \
		echo "Downloading uClibc-ng-1.0.43..."; \
		if [ -f $(UCLIBC_NG_LOCAL) ]; then \
			echo "Found local uClibc-ng-1.0.43.tar.xz, copying..."; \
			cp $(UCLIBC_NG_LOCAL) $(CT_DIR)/.build/tarballs/uClibc-ng-1.0.43.tar.xz; \
		else \
			curl -fSsLo $(CT_DIR)/.build/tarballs/uClibc-ng-1.0.43.tar.xz $(UCLIBC_NG_OFFICIAL); \
		fi; \
	fi
	@echo "Pre-downloading Linux kernel source..."
	@if [ ! -f $(CT_DIR)/.build/tarballs/linux-4.4.x.tar.xz ]; then \
		echo "Linux kernel source will be used from custom location"; \
	fi
	@(cd $(CT_DIR); \
		export CC="${HOMEBREW_PREFIX}/opt/gcc/bin/gcc"; \
		export CXX="${HOMEBREW_PREFIX}/opt/gcc/bin/g++"; \
		export LD="${HOMEBREW_PREFIX}/opt/binutils/bin/ld"; \
		export AR="${HOMEBREW_PREFIX}/opt/binutils/bin/ar"; \
		export AS="${HOMEBREW_PREFIX}/opt/binutils/bin/as"; \
		export NM="${HOMEBREW_PREFIX}/opt/binutils/bin/nm"; \
		export RANLIB="${HOMEBREW_PREFIX}/opt/binutils/bin/ranlib"; \
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
	endif
ifeq ($(CT_TARGET),mipsel-linux-musl)
	@if [ ! -f $(CT_PREFIX)/$(CT_TARGET)/sysroot/usr/include/sys/queue.h ]; then \
		echo "Installing sys/queue.h..."; \
		if [ -f $(TOPDIR)/files/musl/sys/queue.h ]; then \
			cp $(TOPDIR)/files/musl/sys/queue.h $(CT_PREFIX)/$(CT_TARGET)/sysroot/usr/include/sys/queue.h; \
		elif curl -fSsL -o $(CT_PREFIX)/$(CT_TARGET)/sysroot/usr/include/sys/queue.h \
			"https://github.com/tekintian/padavan/raw/refs/heads/main/toolchain/files/musl/sys/queue.h" ; then \
			: ; \
		else \
			echo "Warning: Failed to install sys/queue.h"; \
		fi; \
	fi
endif