#!/bin/bash

echo "=== SNI模块编译测试 ==="

# 进入build目录
cd /padavan/trunk/build/iptables-1.8.7

# 创建必要的头文件
echo "📝 创建xtables-version.h..."
echo '#define XTABLES_VERSION "1.8.7"' > include/xtables-version.h

# 编译SNI扩展
echo "🔧 编译SNI扩展..."
cd extensions
gcc -shared -fPIC -I../include -I../include/xtables -I../../../linux-4.4.x/include/uapi -I../../../linux-4.4.x/include -o libxt_sni.so libxt_sni.c

if [ $? -eq 0 ]; then
    echo "✅ SNI扩展编译成功！"
    echo "📁 文件信息："
    ls -la libxt_sni.so
    file libxt_sni.so
else
    echo "❌ SNI扩展编译失败！"
    exit 1
fi

# 测试SNI扩展的符号
echo "🔍 检查SNI扩展符号..."
nm -D libxt_sni.so | grep _init || echo "⚠️  未找到_init符号"

echo ""
echo "=== 编译测试完成 ==="