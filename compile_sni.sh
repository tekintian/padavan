#!/bin/bash

# SNI模块编译脚本
# 用于在本地Docker CI模拟环境中单独编译SNI扩展

set -e

echo "=== SNI模块单独编译脚本 ==="

# 检查环境
if [ ! -d "/padavan/trunk" ]; then
    echo "❌ 错误: /padavan/trunk 目录不存在"
    echo "请确保在正确的Padavan项目目录中运行此脚本"
    exit 1
fi

# 设置环境变量
export ROOTDIR=/padavan
export BUILD_DIR=$ROOTDIR/trunk/build
export IPTABLES_DIR=$BUILD_DIR/iptables-1.8.7
export EXTENSIONS_DIR=$IPTABLES_DIR/extensions

echo "🔧 步骤1: 检查必要文件..."
if [ ! -f "$ROOTDIR/trunk/user/sni/libxt_sni.c" ]; then
    echo "❌ 错误: SNI源码文件不存在 $ROOTDIR/trunk/user/sni/libxt_sni.c"
    exit 1
fi

if [ ! -f "$ROOTDIR/trunk/linux-4.4.x/include/uapi/linux/netfilter/xt_sni.h" ]; then
    echo "❌ 错误: SNI头文件不存在 $ROOTDIR/trunk/linux-4.4.x/include/uapi/linux/netfilter/xt_sni.h"
    exit 1
fi

echo "✅ 必要文件检查通过"

echo "🔧 步骤2: 准备iptables构建环境..."
# 进入构建目录
cd $IPTABLES_DIR

# 创建必要的头文件
if [ ! -f "include/xtables-version.h" ]; then
    echo '#define XTABLES_VERSION "1.8.7"' > include/xtables-version.h
    echo "✅ 创建 xtables-version.h"
fi

# 确保include/linux/netfilter目录存在
mkdir -p include/linux/netfilter

echo "📁 步骤3: 复制SNI扩展文件..."
# 复制源码文件
cp $ROOTDIR/trunk/user/sni/libxt_sni.c extensions/
echo "✅ 复制 libxt_sni.c"

# 复制头文件
cp $ROOTDIR/trunk/linux-4.4.x/include/uapi/linux/netfilter/xt_sni.h include/linux/netfilter/
echo "✅ 复制 xt_sni.h"

echo "🔨 步骤4: 编译SNI扩展..."
cd extensions

# 清理旧文件
rm -f libxt_sni.so

# 编译命令
gcc -shared -fPIC \
    -I../include \
    -I../include/xtables \
    -I../../../linux-4.4.x/include/uapi \
    -I../../../linux-4.4.x/include \
    -o libxt_sni.so libxt_sni.c

if [ $? -eq 0 ]; then
    echo "✅ SNI扩展编译成功！"
else
    echo "❌ SNI扩展编译失败！"
    exit 1
fi

echo "🔍 步骤5: 验证编译结果..."
# 检查文件信息
if [ -f "libxt_sni.so" ]; then
    echo "✅ 编译产物存在"
    echo "📊 文件信息："
    echo "  - 文件大小: $(stat -c%s libxt_sni.so) bytes"
    echo "  - 文件类型: $(file libxt_sni.so)"
    
    # 检查符号
    echo "🔍 符号验证："
    if nm -D libxt_sni.so | grep -q "_INIT"; then
        echo "  ✅ 初始化符号 (_INIT) 存在"
    else
        echo "  ⚠️  初始化符号 (_INIT) 未找到"
    fi
    
    xtables_symbols=$(nm -D libxt_sni.so | grep -c "xtables")
    echo "  - xtables相关符号: $xtables_symbols 个"
    
    echo "🔗 动态链接依赖："
    ldd libxt_sni.so
    
else
    echo "❌ 编译产物不存在"
    exit 1
fi

echo "📦 步骤6: 安装到目标目录..."
# 创建目标目录
TARGET_DIR=$BUILD_DIR/romfs/usr/lib/xtables
mkdir -p $TARGET_DIR

# 复制编译产物
cp libxt_sni.so $TARGET_DIR/

if [ -f "$TARGET_DIR/libxt_sni.so" ]; then
    echo "✅ 安装成功: $TARGET_DIR/libxt_sni.so"
else
    echo "❌ 安装失败"
    exit 1
fi

echo ""
echo "📋 编译总结"
echo "================================"
echo "模块名称: SNI iptables扩展"
echo "编译环境: $(gcc --version | head -1)"
echo "编译时间: $(date)"
echo "输出文件: libxt_sni.so ($(stat -c%s libxt_sni.so) bytes)"
echo "安装位置: $TARGET_DIR/libxt_sni.so"
echo ""
echo "✅ 编译状态: 成功"
echo "================================"
echo ""
echo "🎯 使用说明:"
echo "1. 此编译产物用于x86_64环境测试"
echo "2. 实际部署需要MIPS交叉编译"
echo "3. 可通过 'iptables -m sni --help' 测试扩展加载"
echo ""
echo "=== 编译完成 ==="