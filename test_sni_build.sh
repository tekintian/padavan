#!/bin/bash

echo "=== 测试 SNI 模块编译 ==="

# 进入项目目录
cd /Volumes/csdisk/padavan/trunk

# 检查配置
if [ ! -f .config ]; then
    echo "复制配置文件..."
    cp configs/templates/K2P.config .config
fi

# 检查 SNI 配置是否启用
grep -q "CONFIG_FIRMWARE_INCLUDE_SNI_FILTER=y" .config
if [ $? -eq 0 ]; then
    echo "✅ SNI 过滤器已启用"
else
    echo "❌ SNI 过滤器未启用"
    exit 1
fi

# 尝试编译
echo "开始编译..."
make clean > /dev/null 2>&1
make K2P 2>&1 | tee /tmp/sni_build.log

# 检查是否成功
if [ $? -eq 0 ]; then
    echo "✅ 编译成功"
else
    echo "❌ 编译失败"
    echo "查看错误日志:"
    grep -A 10 -B 5 "error:" /tmp/sni_build.log | head -20
fi