# AdByBy-Open Makefile
CC = $(CROSS_COMPILE)gcc
STRIP = $(CROSS_COMPILE)strip
CFLAGS = -Wall -Wextra -O2 -s
TARGET = adbyby
LDFLAGS = 
SRCDIR = .
OBJDIR = obj
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# 默认目标
all: $(TARGET)

# 创建目录
$(OBJDIR):
	mkdir -p $(OBJDIR)

# 编译目标文件
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接可执行文件
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@
	$(STRIP) $(TARGET)

# 清理
clean:
	rm -rf $(OBJDIR) $(TARGET)

# 安装到romfs目录
romfs:
	$(ROMFSINST) -p +x $(THISDIR)/$(TARGET) /usr/share/adbyby/

# 调试版本
debug: CFLAGS += -g -DDEBUG
debug: clean all

.PHONY: all clean romfs debug