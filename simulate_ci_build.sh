#!/bin/bash
# 模拟 GitHub CI 构建环境测试

echo "=== Simulating GitHub CI Build ==="

# 检查Docker镜像
if ! docker images | grep -q "tekintian/padavan-compiler"; then
    echo "Pulling padavan-compiler Docker image..."
    docker pull tekintian/padavan-compiler:4.4.198
fi

# 模拟完整的构建流程
docker run --rm \
    -v /Volumes/csdisk/padavan/trunk:/padavan/trunk \
    tekintian/padavan-compiler:4.4.198 \
    bash -c "
        cd /padavan/trunk
        
        echo '=== Environment Setup ==='
        export ARCH=mips
        export CROSS_COMPILE=mipsel-linux-musl-
        
        echo '=== Building User Space Components ==='
        # 只构建 iptables 扩展部分来测试
        cd user/iptables/iptables-1.8.7
        
        echo '=== Configuring iptables build ==='
        if [ ! -f Makefile ]; then
            ./configure --host=mipsel-linux-musl --disable-shared --enable-static
        fi
        
        echo '=== Building extensions ==='
        cd extensions
        
        echo '=== Testing libxt_sni compilation ==='
        make libxt_sni.o 2>&1 | head -20
        
        if [ -f libxt_sni.o ]; then
            echo '✅ libxt_sni.o compiled successfully!'
            file libxt_sni.o
        else
            echo '❌ libxt_sni.o compilation failed'
        fi
        
        echo '=== Checking for other compilation errors ==='
        make 2>&1 | grep -i error | head -10 || echo 'No errors found'
    "

echo "=== CI Simulation completed ==="