#!/bin/bash
# SNI模块Docker编译测试脚本

echo "=== Testing SNI module compilation in Docker ==="

# 检查Docker是否运行
if ! docker ps >/dev/null 2>&1; then
    echo "Error: Docker is not running. Please start Docker Desktop."
    exit 1
fi

# 使用现有的Padavan编译环境
echo "Pulling/using Padavan compiler Docker image..."
docker pull tekintian/padavan-compiler:latest 2>/dev/null

echo "Testing SNI module compilation..."
docker run --rm -v "$(pwd):/padavan" -w /padavan tekintian/padavan-compiler sh -c "
    set -e
    
    echo '=== Environment check ==='
    which mipsel-linux-musl-gcc
    echo 'Cross-compiler found.'
    
    echo '=== Build system integration test ==='
    cd /padavan/trunk/user
    
    # 检查SNI是否在构建配置中
    echo 'Checking if SNI is configured in build system...'
    if grep -q 'CONFIG_FIRMWARE_INCLUDE_SNI_FILTER.*sni' Makefile; then
        echo '✅ SNI module properly integrated in Makefile'
    else
        echo '❌ SNI module not found in Makefile'
        exit 1
    fi
    
    # 尝试编译SNI模块
    echo 'Attempting to compile SNI module...'
    if make sni_only; then
        echo '✅ SNI module compiled successfully'
        
        # 检查生成的文件
        if [ -f sni/libxt_sni.so ]; then
            echo '✅ libxt_sni.so generated'
            file sni/libxt_sni.so
            ls -la sni/libxt_sni.so
        else
            echo '❌ libxt_sni.so not found'
        fi
    else
        echo '❌ SNI module compilation failed'
        exit 1
    fi
    
    echo '=== CI Compilation test completed successfully ==='
"

echo "=== Docker compilation test completed ==="