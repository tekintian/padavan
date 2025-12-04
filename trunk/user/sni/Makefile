include ../../.config
include ../../rules.mk

# 调整CFLAGS，确保头文件路径正确
CFLAGS += -I../../include -I../../linux-4.4.x/include -I../../linux-4.4.x/user_headers/include -I../../build/iptables-1.8.7/include

# 源文件和目标文件定义
SRCS = libxt_sni.c
OBJS = $(SRCS:.c=.o)
TARGET = libxt_sni.so

# 主目标
all: $(TARGET)

# 确保在build目录中也能找到源文件
$(BUILD_DIR)/sni/%.c:
	cp -f $(CURDIR)/$*.c $@

# 动态库构建规则
$(TARGET): $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

# 清理规则
clean:
	rm -f *.o *.so

# 安装规则
romfs:
	$(INSTALL_DIR) $(ROMFSDIR)/usr/lib/xtables
	$(INSTALL_BIN) $(TARGET) $(ROMFSDIR)/usr/lib/xtables/