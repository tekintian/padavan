#!/bin/bash
# SNI模块本地编译脚本 - 修复版本
# 获取当前脚本的绝对路径
CURRENT_DIR=$(cd "$(dirname "$0")" && pwd)

echo "=== SNI Filter Module Local Compilation (Fixed) ==="
echo

# 检查Docker是否可用
if ! command -v docker &> /dev/null; then
    echo "Error: Docker is not installed or not in PATH"
    exit 1
fi

# 检查Docker镜像是否存在
echo "Checking for padavan-compiler Docker image..."
if ! docker images | grep -q "tekintian/padavan-compiler"; then
    echo "Pulling padavan-compiler Docker image..."
    docker pull tekintian/padavan-compiler:4.4.198
    if [ $? -ne 0 ]; then
        echo "Error: Failed to pull Docker image"
        exit 1
    fi
fi

# 检查源码目录
if [ ! -d "${CURRENT_DIR}/trunk" ]; then
    echo "Error: Source directory ${CURRENT_DIR}/trunk not found"
    exit 1
fi

echo "Starting compilation in Docker environment..."
echo

# 运行Docker编译容器 - 修复版本
docker run -it --rm \
    -v ${CURRENT_DIR}/trunk:/padavan/trunk \
    tekintian/padavan-compiler:4.4.198 \
    bash -c "
        cd /padavan/trunk/linux-4.4.x
        
        # 确保配置文件存在且有效
        if [ ! -f .config ]; then
            echo 'Error: .config file not found'
            exit 1
        fi
        
        # 设置架构和环境变量
        export ARCH=mips
        export CROSS_COMPILE=mipsel-linux-musl-
        
        # 检查工具链
        if ! command -v mipsel-linux-musl-gcc &> /dev/null; then
            echo 'Error: mipsel-linux-musl-gcc not found in PATH'
            echo 'Available compilers:'
            find /opt -name '*gcc*' 2>/dev/null | head -5
            exit 1
        fi
        
        echo 'Toolchain found:'
        which mipsel-linux-musl-gcc
        mipsel-linux-musl-gcc --version | head -1
        
        echo
        echo 'Preparing kernel build environment...'
        
        # 准备构建环境
        make modules_prepare
        
        echo
        echo 'Compiling SNI module only...'
        
        # 只编译SNI模块
        make M=net/netfilter/xt_sni_filter.o
        
        echo
        echo 'Checking compilation results...'
        if [ -f net/netfilter/xt_sni_filter.o ]; then
            echo '✓ SNI module object file created successfully'
            ls -la net/netfilter/xt_sni_filter.*
        else
            echo '✗ SNI module compilation failed'
            echo 'Trying alternative compilation method...'
            
            # 备用方法：直接编译单个文件
            mipsel-linux-musl-gcc -D__KERNEL__ -DMODULE -I./include -I./arch/mips/include -I./net/netfilter -c net/netfilter/xt_sni_filter.c -o net/netfilter/xt_sni_filter.o
            
            if [ -f net/netfilter/xt_sni_filter.o ]; then
                echo '✓ SNI module compiled with alternative method'
                ls -la net/netfilter/xt_sni_filter.*
            else
                echo '✗ All compilation methods failed'
                exit 1
            fi
        fi
        
        echo
        echo 'Compilation completed.'
    "

echo
echo "=== Compilation process completed ==="
echo "Check the output above for compilation status."