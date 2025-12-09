# AdByBy-Open Padavan Makefile
# 支持固件构建时自动编译安装和独立编译两种模式

THISDIR = $(shell pwd)
SRCDIR = $(THISDIR)/src
TARGET_NAME = adbyby
TARGET = $(SRCDIR)/$(TARGET_NAME)

# 编译器设置（使用项目定义的交叉编译器变量）
# 在固件构建环境中，这些变量由上层Makefile提供
CC = $(TARGET_CC)
STRIP = $(TARGET_CROSS)strip
AR = $(TARGET_AR)

# 如果在独立开发环境中，可以手动指定交叉编译器
ifndef TARGET_CC
    ifeq ($(CROSS_COMPILE),)
        # 本地开发环境使用gcc
        CC = gcc
        STRIP = strip
        AR = ar
    else
        # 手动指定的交叉编译器
        CC = $(CROSS_COMPILE)gcc
        STRIP = $(CROSS_COMPILE)strip
        AR = $(CROSS_COMPILE)ar
    endif
endif

# 编译标志
CFLAGS = -Wall -Wextra -O2
LDFLAGS = 

# 检测是否为Padavan构建环境（通过ROMFSINST变量判断）
ifdef ROMFSINST
    BUILD_MODE = romfs
    TARGET = $(SRCDIR)/$(TARGET_NAME)
    INSTALL_TARGET = $(THISDIR)/share/$(TARGET_NAME)
else
    BUILD_MODE = standalone
    TARGET = $(THISDIR)/$(TARGET_NAME)
endif

# 源文件和目标文件
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(SRCDIR)/obj/%.o)

# 默认目标
all: compile

# 编译目标
compile: $(TARGET)

# 创建对象目录
$(SRCDIR)/obj:
	@mkdir -p $(SRCDIR)/obj

# 编译C文件
$(SRCDIR)/obj/%.o: $(SRCDIR)/%.c | $(SRCDIR)/obj
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# 链接生成可执行文件
$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET_NAME)..."
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@
	$(STRIP) $@
	@echo "Build completed: $@"

# Padavan固件构建模式下的特殊处理
ifeq ($(BUILD_MODE),romfs)
    # 固件构建模式：先编译，再复制到share目录，再安装到ROMFS
    romfs: compile install-romfs
    
    # 复制编译结果到share目录（为ROMFS安装准备）
    install-romfs: 
	@echo "Installing compiled binary to share directory..."
	cp -f $(TARGET) $(INSTALL_TARGET) || true
	@echo "Note: If cross-compilation is not working, the pre-built binary will be used"
	@echo "Starting ROMFS installation..."
	# 创建所有需要的目录
	mkdir -p $(ROMFSDIR)/etc_ro 2>/dev/null || true
	mkdir -p $(ROMFSDIR)/usr/share/adbyby 2>/dev/null || true
	mkdir -p $(ROMFSDIR)/usr/share/adbyby/data 2>/dev/null || true
	mkdir -p $(ROMFSDIR)/usr/share/adbyby/doc 2>/dev/null || true
	mkdir -p $(ROMFSDIR)/usr/bin 2>/dev/null || true

	# 安装编译后的主程序文件（可执行文件）
	$(ROMFSINST) -p +x "$(INSTALL_TARGET)" /usr/share/adbyby/ || true

	# 安装配置文件
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

	# 安装脚本文件（带执行权限）
	$(ROMFSINST) -p +x "$(THISDIR)/share/adblock.sh" /usr/share/adbyby/ || true
	$(ROMFSINST) -p +x "$(THISDIR)/share/adbyby.sh" /usr/share/adbyby/ || true
	$(ROMFSINST) -p +x "$(THISDIR)/share/adbybyfirst.sh" /usr/share/adbyby/ || true
	$(ROMFSINST) -p +x "$(THISDIR)/share/adbybyupdate.sh" /usr/share/adbyby/ || true
	$(ROMFSINST) -p +x "$(THISDIR)/share/admem.sh" /usr/share/adbyby/ || true
	$(ROMFSINST) -p +x "$(THISDIR)/share/adupdate.sh" /usr/share/adbyby/ || true

	# 安装data目录文件
	$(ROMFSINST) "$(THISDIR)/share/data/lazy.txt" /usr/share/adbyby/data/ || true
	$(ROMFSINST) "$(THISDIR)/share/data/rules.txt" /usr/share/adbyby/data/ || true
	$(ROMFSINST) "$(THISDIR)/share/data/user.txt" /usr/share/adbyby/data/ || true
	$(ROMFSINST) "$(THISDIR)/share/data/video.txt" /usr/share/adbyby/data/ || true

	# 安装doc目录文件
	$(ROMFSINST) "$(THISDIR)/share/doc/hidecss.js" /usr/share/adbyby/doc/ || true

	# 安装自定义配置脚本
	$(ROMFSINST) "$(THISDIR)/adbyby_rules.sh" /etc_ro/ || true
	$(ROMFSINST) "$(THISDIR)/adbyby_adblack.sh" /etc_ro/ || true
	$(ROMFSINST) "$(THISDIR)/adbyby_adesc.sh" /etc_ro/ || true
	$(ROMFSINST) "$(THISDIR)/adbyby_adhost.sh" /etc_ro/ || true
	$(ROMFSINST) "$(THISDIR)/adbyby_host.sh" /etc_ro/ || true
	$(ROMFSINST) "$(THISDIR)/adbyby_blockip.sh" /etc_ro/ || true
	# 复制主脚本到/usr/bin
	$(ROMFSINST) -p +x "$(THISDIR)/adbyby.sh" /usr/bin/adbyby.sh || true
	@echo "ROMFS installation completed!"

else
    # 独立模式：只编译到当前目录
    romfs:
	@echo "Error: ROMFS mode not available. This is standalone build mode."
	@echo "Please run: make compile"
	@exit 1
endif

# 清理编译文件
clean:
	@echo "Cleaning build files..."
	rm -rf $(SRCDIR)/obj $(TARGET) $(THISDIR)/$(TARGET_NAME) || true
	@echo "Clean completed!"

# 深度清理（包括安装的文件）
distclean: clean
	@echo "Deep cleaning all generated files..."
	rm -f $(THISDIR)/share/$(TARGET_NAME) || true
	@echo "Distclean completed!"

# 调试版本编译
debug: CFLAGS += -g -DDEBUG
debug: clean compile

# 发布版本编译
release: CFLAGS += -DNDEBUG -Os
release: clean compile

# 查看构建信息
info:
	@echo "=== AdByBy Build Information ==="
	@echo "Build Mode: $(BUILD_MODE)"
	@echo "Target: $(TARGET)"
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"
	@echo "Sources: $(SOURCES)"
	@echo "Objects: $(OBJECTS)"
	@echo "==============================="

# 测试编译环境
test-env:
	@echo "Testing build environment..."
	@echo "CROSS_COMPILE: $(CROSS_COMPILE)"
	@echo "CC: $(CC)"
	@echo "ROMFSINST: $(ROMFSINST)"
	@echo "BUILD_MODE: $(BUILD_MODE)"
	@which $(CC) || echo "Compiler not found!"

# 快速重建（仅编译修改的文件）
rebuild: clean compile

# 安装到本地（用于测试）
install-local: compile
	@echo "Installing to /usr/local/bin/..."
	sudo cp -f $(TARGET) /usr/local/bin/adbyby || true
	@echo "Local installation completed!"

.PHONY: all compile clean distclean debug release info test-env install-local rebuild romfs install-romfs