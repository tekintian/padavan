#!/bin/bash
# 简单的SNI模块结构和依赖检查脚本

echo "=== SNI Module Structure Verification ==="

# 检查SNI模块文件是否完整
echo "1. Checking SNI module files..."

# 检查用户空间库
if [ -f "trunk/user/sni/libxt_sni.c" ]; then
    echo "   ✅ Userspace library: libxt_sni.c exists"
    echo "   Size: $(wc -c < trunk/user/sni/libxt_sni.c) bytes"
else
    echo "   ❌ Userspace library: libxt_sni.c missing"
fi

# 检查内核模块
if [ -f "trunk/linux-4.4.x/net/netfilter/xt_sni.c" ]; then
    echo "   ✅ Kernel module: xt_sni.c exists"
    echo "   Size: $(wc -c < trunk/linux-4.4.x/net/netfilter/xt_sni.c) bytes"
else
    echo "   ❌ Kernel module: xt_sni.c missing"
fi

# 检查头文件
if [ -f "trunk/linux-4.4.x/include/uapi/linux/netfilter/xt_sni.h" ]; then
    echo "   ✅ Header file: xt_sni.h exists"
    echo "   Size: $(wc -c < trunk/linux-4.4.x/include/uapi/linux/netfilter/xt_sni.h) bytes"
else
    echo "   ❌ Header file: xt_sni.h missing"
fi

echo ""
echo "2. Checking build system integration..."

# 检查Makefile
if grep -q "CONFIG_FIRMWARE_INCLUDE_SNI_FILTER.*sni" trunk/user/Makefile; then
    echo "   ✅ SNI module integrated in user/Makefile"
else
    echo "   ❌ SNI module not found in user/Makefile"
fi

# 检查内核Makefile
if grep -q "CONFIG_NETFILTER_XT_MATCH_SNI.*xt_sni.o" trunk/linux-4.4.x/net/netfilter/Makefile; then
    echo "   ✅ SNI module integrated in kernel Makefile"
else
    echo "   ❌ SNI module not found in kernel Makefile"
fi

# 检查Kconfig
if grep -q "config NETFILTER_XT_MATCH_SNI" trunk/linux-4.4.x/net/netfilter/Kconfig; then
    echo "   ✅ SNI module Kconfig present"
else
    echo "   ❌ SNI module Kconfig missing"
fi

echo ""
echo "3. Checking board configurations..."

# 计算配置SNI的板子数量
sni_configs=$(find trunk/configs/boards -name "kernel-4.4.x.config" -exec grep -l "CONFIG_NETFILTER_XT_MATCH_SNI=" {} \; | wc -l)
echo "   📊 Boards with SNI kernel config: $sni_configs"

# 检查用户空间配置
user_sni_configs=$(find trunk/configs -name "*.config" -exec grep -l "CONFIG_FIRMWARE_INCLUDE_SNI_FILTER" {} \; | wc -l)
echo "   📊 Config templates with SNI filter: $user_sni_configs"

echo ""
echo "4. Checking for potential issues..."

# 检查是否有语法错误
echo "   🔍 Checking for basic syntax issues in libxt_sni.c..."
if [ -f "trunk/user/sni/libxt_sni.c" ]; then
    # 检查括号匹配
    open_braces=$(grep -o '{' trunk/user/sni/libxt_sni.c | wc -l)
    close_braces=$(grep -o '}' trunk/user/sni/libxt_sni.c | wc -l)
    if [ $open_braces -eq $close_braces ]; then
        echo "   ✅ Brace matching in libxt_sni.c: OK"
    else
        echo "   ⚠️  Brace mismatch in libxt_sni.c: $open_braces open, $close_braces close"
    fi
    
    # 检查函数定义
    functions=$(grep -E '^[a-zA-Z_][a-zA-Z0-9_]*\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\(' trunk/user/sni/libxt_sni.c | wc -l)
    echo "   📊 Functions in libxt_sni.c: $functions"
fi

if [ -f "trunk/linux-4.4.x/net/netfilter/xt_sni.c" ]; then
    echo "   🔍 Checking for basic syntax issues in xt_sni.c..."
    open_braces=$(grep -o '{' trunk/linux-4.4.x/net/netfilter/xt_sni.c | wc -l)
    close_braces=$(grep -o '}' trunk/linux-4.4.x/net/netfilter/xt_sni.c | wc -l)
    if [ $open_braces -eq $close_braces ]; then
        echo "   ✅ Brace matching in xt_sni.c: OK"
    else
        echo "   ⚠️  Brace mismatch in xt_sni.c: $open_braces open, $close_braces close"
    fi
fi

echo ""
echo "=== Verification completed ==="
echo ""
echo "For full compilation test, run in a Padavan Docker environment:"
echo "docker run --rm -v \$(pwd):/padavan -w /padavan ghcr.io/hanwckf/padavan-build-env:latest sh -c 'cd trunk/user && make sni_only'"