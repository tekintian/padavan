#!/bin/bash
# compile_sni_module.sh
CURRENT_DIR=$(cd "$(dirname "$0")" && pwd)

echo "开始编译SNI模块..."

# 设置环境变量
export ARCH=mips
export CROSS_COMPILE=mipsel-linux-musl-

# 进入工作目录
cd ${CURRENT_DIR}/linux-4.4.x

# 清理之前的编译结果
make M=net/netfilter clean

# 编译模块
make M=net/netfilter modules

# 检查编译结果
if [ -f "net/netfilter/xt_sni_filter.ko" ]; then
    echo "编译成功！模块位置：net/netfilter/xt_sni_filter.ko"
    ls -la net/netfilter/xt_sni_filter.ko
else
    echo "编译失败，请检查错误信息"
fi