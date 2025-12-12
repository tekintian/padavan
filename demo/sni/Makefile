include ../../.config
include ../../rules.mk

# SNI模块Makefile - 基于STRING模块改进

# 定义目标名称
TARGET := libxt_sni.so

# 定义源文件
SRCS := libxt_sni.c
OBJS := $(SRCS:.c=.o)

# 添加必要的包含目录 - 支持多种路径
ifneq ($(TOPDIR),)
  CFLAGS += -I$(TOPDIR)/linux-4.4.x/include/uapi
  CFLAGS += -I$(TOPDIR)/user/iptables/iptables-1.8.7/include
  CFLAGS += -I$(TOPDIR)/user/iptables/iptables-1.8.7/libiptc
  CFLAGS += -I$(TOPDIR)/user/iptables/iptables-1.8.7/libxtables
else
  # 本地路径
  CFLAGS += -I../../linux-4.4.x/include/uapi
  CFLAGS += -I../iptables/iptables-1.8.7/include
  CFLAGS += -I../iptables/iptables-1.8.7/libiptc
  CFLAGS += -I../iptables/iptables-1.8.7/libxtables

  # Docker容器路径
  CFLAGS += -I../../build/iptables-1.8.7/include
  CFLAGS += -I../../../build/iptables-1.8.7/include
endif
CFLAGS += -fPIC -shared

# 构建规则
all: $(TARGET)

# 编译目标文件
$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 链接共享库
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -shared -o $@ $^ -lxtables

# 确保构建目录中有源代码
$(SRCS):
	@echo "确保源文件存在: $@"
	@if [ -f "$@" ]; then \
	    echo "源文件 $@ 已存在"; \
	else \
	    echo "错误: 源文件 $@ 不存在"; \
	    exit 1; \
	fi

# 安装规则 - 与STRING模块保持一致
romfs: $(TARGET)
	$(ROMFSINST) -d $(TARGET) /usr/lib/xtables/

# 清理规则
clean:
	rm -f $(OBJS) $(TARGET)

# 辅助目标 - 检查编译环境
check-env:
	@echo "检查编译环境..."
	@if [ -z "$(TOPDIR)" ]; then \
	    echo "警告: TOPDIR环境变量未设置，可能导致编译失败"; \
	    echo "建议设置: export TOPDIR=/Volumes/csdisk/padavan"; \
	fi
	@echo "包含目录: $(CFLAGS)"

# 依赖文件检查
check-deps:
	@echo "检查依赖文件..."
	@if [ ! -f "$(TOPDIR)/linux-4.4.x/include/uapi/linux/netfilter/xt_sni.h" ]; then \
	    echo "错误: 缺少头文件 xt_sni.h"; \
	    exit 1; \
	fi
	@echo "依赖文件检查通过"

# 伪目标
.PHONY: all clean romfs check-env check-deps