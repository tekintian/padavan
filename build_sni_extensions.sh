#!/bin/bash
# SNI扩展库编译和安装脚本
# 修复iptables-restore "Couldn't find match `sni'" 错误

echo "=== SNI Extension Library Build and Install Script ==="
echo

# 获取当前脚本路径
CURRENT_DIR=$(cd "$(dirname "$0")" && pwd)
IPTABLES_DIR="${CURRENT_DIR}/trunk/user/iptables/iptables-1.8.7"

# 检查源码目录
if [ ! -d "$IPTABLES_DIR" ]; then
    echo "Error: iptables source directory not found: $IPTABLES_DIR"
    exit 1
fi

# 检查Docker是否可用
if ! command -v docker &> /dev/null; then
    echo "Error: Docker is not installed or not in PATH"
    exit 1
fi

# 检查Docker镜像
echo "Checking for padavan-compiler Docker image..."
if ! docker images | grep -q "tekintian/padavan-compiler"; then
    echo "Pulling padavan-compiler Docker image..."
    docker pull tekintian/padavan-compiler:4.4.198
    if [ $? -ne 0 ]; then
        echo "Error: Failed to pull Docker image"
        exit 1
    fi
fi

echo "Starting SNI extension library compilation..."
echo

# 运行Docker编译容器
docker run -it --rm \
    -v ${CURRENT_DIR}/trunk:/padavan/trunk \
    tekintian/padavan-compiler:4.4.198 \
    bash -c "
        cd /padavan/trunk/user/iptables/iptables-1.8.7
        
        # 设置环境变量
        export CC=mipsel-linux-musl-gcc
        export CFLAGS='-Os -fPIC'
        export LDFLAGS=''
        
        echo 'Configuring iptables...'
        
        # 配置构建环境
        ./configure \
            --host=mipsel-linux \
            --disable-static \
            --enable-shared \
            --disable-nftables \
            --disable-connlabel \
            --with-xtlibdir=/usr/lib/xtables \
            --with-kernel=/padavan/trunk/linux-4.4.x \
            --with-xt-lock-name='/var/lock/xtables.lock'
        
        echo
        echo 'Building iptables and extensions...'
        
        # 使用构建系统编译整个项目
        make
        
        echo
        echo 'Building specific extension libraries...'
        
        # 进入扩展目录编译我们的扩展
        cd extensions
        
        # 使用构建系统的 Makefile 编译特定扩展
        make libxt_sni.so libxt_webstr.so
        
        echo
        echo 'Checking compilation results...'
        
        if [ -f libxt_sni.so ]; then
            echo '✓ libxt_sni.so compiled successfully'
            ls -la libxt_sni.so
            file libxt_sni.so
        else
            echo '✗ libxt_sni.so compilation failed'
            echo 'Available .so files:'
            ls -la *.so 2>/dev/null || echo 'No .so files found'
            exit 1
        fi
        
        if [ -f libxt_webstr.so ]; then
            echo '✓ libxt_webstr.so compiled successfully'
            ls -la libxt_webstr.so
            file libxt_webstr.so
        else
            echo '✗ libxt_webstr.so compilation failed'
            echo 'Available .so files:'
            ls -la *.so 2>/dev/null || echo 'No .so files found'
            exit 1
        fi
        
        echo
        echo 'Copying libraries to host system...'
        
        # 复制到主机系统（在容器内挂载的路径）
        cp libxt_sni.so /padavan/trunk/
        cp libxt_webstr.so /padavan/trunk/
        
        echo '✓ Libraries copied to host system'
    "

if [ $? -eq 0 ]; then
    echo
    echo "=== Compilation completed successfully ==="
    echo "Extension libraries are ready for deployment."
    echo
    
    # 检查编译结果
    if [ -f "${CURRENT_DIR}/trunk/libxt_sni.so" ] && [ -f "${CURRENT_DIR}/trunk/libxt_webstr.so" ]; then
        echo "✓ Extension libraries found:"
        ls -la "${CURRENT_DIR}/trunk/libxt_sni.so"
        ls -la "${CURRENT_DIR}/trunk/libxt_webstr.so"
        echo
        
        echo "To install these libraries on your router:"
        echo "1. Copy the files to the router:"
        echo "   scp ${CURRENT_DIR}/trunk/libxt_sni.so admin@192.168.2.1:/tmp/"
        echo "   scp ${CURRENT_DIR}/trunk/libxt_webstr.so admin@192.168.2.1:/tmp/"
        echo
        echo "2. Install them on the router:"
        echo "   ssh admin@192.168.2.1 'mkdir -p /usr/lib/xtables && mv /tmp/libxt_sni.so /usr/lib/xtables/ && mv /tmp/libxt_webstr.so /usr/lib/xtables/'"
        echo
        echo "3. Restart the firewall:"
        echo "   ssh admin@192.168.2.1 'restart_firewall'"
        echo
    else
        echo "✗ Extension libraries not found after compilation"
        exit 1
    fi
else
    echo "✗ Compilation failed"
    exit 1
fi