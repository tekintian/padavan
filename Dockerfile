# 基础镜像（锁定 22.04，杜绝版本兼容问题| 国内加速地址 m.daocloud.io/docker.io/ubuntu:22.04）
FROM ubuntu:22.04

# 镜像元数据（明确标注功能特性）
LABEL maintainer="TEKINTIAN <tekintian@gmail.com>"
LABEL description="Padavan 固件编译环境（工具链：mipsel-linux-musl_4.4.x，支持多型号参数化编译）"
LABEL version="4.4.198"
LABEL toolchain_package="mipsel-linux-musl_4.4.x.tar.xz"
LABEL toolchain_actual_path="/usr/local/mipsel-toolchain-4.4.x"
LABEL toolchain_link_path="/padavan/toolchain/toolchain-mipsel/toolchain-4.4.x"
LABEL ci_workdir="/padavan"
LABEL status="稳定可用，支持参数化编译脚本，环境变量全局生效"

# 环境变量（直接加入工具链 PATH，全局生效，无需启动时再设置）
ENV DEBIAN_FRONTEND=noninteractive \
    LC_ALL=en_US.UTF-8 \
    LANG=en_US.UTF-8 \
    LANGUAGE=en_US.UTF-8 \
    TZ=Asia/Shanghai \
    # 直接将工具链路径写入 ENV，Docker 全局生效（核心优化）
    PATH="/usr/local/mipsel-toolchain-4.4.x/bin:/padavan/toolchain/toolchain-mipsel/toolchain-4.4.x/bin:$PATH"

# 构建参数（支持自定义 APT 镜像源）
ARG APT_MIRROR_HOST=mirrors.aliyun.com

# 第一步：安装编译依赖 + 系统初始化
RUN set -eux && \
    # 替换 APT 镜像源（国内加速）
    sed -i "s|archive.ubuntu.com|${APT_MIRROR_HOST}|g; s|security.ubuntu.com|${APT_MIRROR_HOST}|g" /etc/apt/sources.list && \
    # 安装所有必需依赖（含 ccache 加速）
    apt update -y -q && \
    apt install -y -q --no-install-recommends \
        build-essential cmake autoconf automake libtool libtool-bin \
        gperf bison flex bc gettext autopoint python3-docutils texinfo help2man \
        zlib1g-dev libssl-dev libncurses5-dev libncursesw5-dev libgmp3-dev libmpc-dev libmpfr-dev libltdl-dev \
        ccache git curl wget unzip cpio fakeroot kmod sudo vim nano htop xxd locales && \
    # 配置时区和编码（避免编译警告）
    ln -snf /usr/share/zoneinfo/${TZ} /etc/localtime && \
    echo ${TZ} > /etc/timezone && \
    echo "en_US.UTF-8 UTF-8" > /etc/locale.gen && \
    locale-gen && \
    # 清理缓存（减少镜像体积）
    apt autoremove -y && \
    apt clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# 复制工具链包到容器（确保包与 Dockerfile 同级）
COPY mipsel-linux-musl_4.4.x.tar.xz /tmp/

# 第二步：安装工具链 + 创建软链接（所有文件已验证存在）
RUN set -eux && \
    # 1. 创建工具链目录并解压（与本地测试结构一致）
    mkdir -p /usr/local/mipsel-toolchain-4.4.x && \
    chmod 755 -R /usr/local/mipsel-toolchain-4.4.x && \
    tar -xJf /tmp/mipsel-linux-musl_4.4.x.tar.xz -C /usr/local/mipsel-toolchain-4.4.x && \
    rm -rf /tmp/mipsel-linux-musl_4.4.x.tar.xz && \
    # 2. 授权所有工具可执行（递归授权，避免遗漏）
    chmod +x -R /usr/local/mipsel-toolchain-4.4.x/bin/ && \
    chmod +x -R /usr/local/mipsel-toolchain-4.4.x/mipsel-linux-musl/bin/ && \
    # 3. 创建 Padavan 源码预期的目录结构和软链接
    mkdir -p /padavan/toolchain/toolchain-mipsel && \
    chmod 755 -R /padavan/toolchain && \
    ln -s /usr/local/mipsel-toolchain-4.4.x /padavan/toolchain/toolchain-mipsel/toolchain-4.4.x && \
    # 4. 关键验证：确保核心工具存在 + 软链接有效
    if [ ! -f "/usr/local/mipsel-toolchain-4.4.x/bin/mipsel-linux-musl-gcc" ] || \
       [ ! -f "/usr/local/mipsel-toolchain-4.4.x/bin/mipsel-linux-musl-ar" ] || \
       [ ! -f "/usr/local/mipsel-toolchain-4.4.x/bin/mipsel-linux-musl-ld" ]; then \
        echo "ERROR: 关键编译工具（gcc/ar/ld）缺失！"; \
        exit 1; \
    fi && \
    if [ ! -f "/padavan/toolchain/toolchain-mipsel/toolchain-4.4.x/bin/mipsel-linux-musl-gcc" ]; then \
        echo "ERROR: 项目路径软链接失效！"; \
        exit 1; \
    fi

# 第三步：最终验证 + 输出环境信息（直接全局调用编译器）
RUN set -eux && \
    # 直接全局调用编译器，验证 PATH 生效
    COMPILER_VERSION=$(mipsel-linux-musl-gcc --version | head -n1) && \
    CCACHE_VERSION=$(ccache --version | head -n1) && \
    echo "======================================" && \
    echo "✅ Padavan 编译环境完全就绪（v2.4）！" && \
    echo "📦 工具链包：mipsel-linux-musl_4.4.x.tar.xz" && \
    echo "🔧 编译器版本：${COMPILER_VERSION}" && \
    echo "💨 缓存工具：${CCACHE_VERSION}" && \
    echo "📁 工具链路径：/usr/local/mipsel-toolchain-4.4.x/bin" && \
    echo "📁 项目路径：/padavan/toolchain/toolchain-mipsel/toolchain-4.4.x/bin" && \
    echo "🌐 全局 PATH：${PATH}" && \
    echo "💡 配套脚本：build_padavan.sh（支持参数传递型号）" && \
    echo "======================================"

# 设置工作目录（与挂载目录一致）
WORKDIR /padavan

# 启动命令（精简，直接进入交互 Shell，PATH 已全局生效）
CMD ["/bin/bash", "-c", \
    "echo '=== Padavan 编译环境（工具链版本 v4.4.198）==='; \
     echo '📦 工具链：mipsel-linux-musl_4.4.x'; \
     echo '📋 工作目录：$(pwd)'; \
     echo '✅ 编译器全局可用：$(mipsel-linux-musl-gcc --version | head -n1 | awk '{print $1,$2,$3}')'; \
     echo '💡 用法：./build_padavan.sh [路由器型号]（如：./build_padavan.sh K2P）'; \
     echo '======================================'; \
     /bin/bash"]