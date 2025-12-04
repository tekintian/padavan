#!/bin/bash
# Docker CI 环境构建测试脚本

set -euo pipefail

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 创建日志目录
mkdir -p logs

echo -e "${BLUE}=== Docker CI 环境构建测试 ===${NC}"
echo ""

# 第1步：构建 Docker 镜像
log_info "构建 Docker 镜像..."
if docker build -f Dockerfile.ci-sim -t padavan-ci-sim . 2>&1 | tee logs/docker_build.log; then
    log_success "Docker 镜像构建成功"
else
    log_error "Docker 镜像构建失败，查看 logs/docker_build.log"
    exit 1
fi

echo ""

# 第2步：测试环境变量
log_info "测试 CI 环境变量..."
if docker run --rm -v $(pwd):/padavan padavan-ci-sim /tmp/test_ci_pollution.sh 2>&1 | tee logs/env_test.log; then
    log_success "环境变量测试完成"
else
    log_error "环境变量测试失败，查看 logs/env_test.log"
fi

echo ""

# 第3步：测试 iptables 编译
log_info "测试 iptables 编译..."
if docker run --rm -v $(pwd):/padavan padavan-ci-sim /tmp/test_iptables_compile.sh 2>&1 | tee logs/iptables_test.log; then
    log_success "iptables 编译测试完成"
else
    log_error "iptables 编译测试失败，查看 logs/iptables_test.log"
fi

echo ""

# 第4步：显示日志文件位置
echo -e "${BLUE}=== 日志文件 ===${NC}"
echo "📋 Docker 构建日志：logs/docker_build.log"
echo "📋 环境变量测试：logs/env_test.log"
echo "📋 iptables 编译日志：logs/iptables_test.log"
echo "📋 详细编译日志：logs/iptables_compile.log"

echo ""
echo -e "${GREEN}=== 测试完成 ===${NC}"
echo ""
echo -e "${YELLOW}手动运行命令：${NC}"
echo "1. 进入容器：docker run -it --rm -v \$(pwd):/padavan padavan-ci-sim"
echo "2. 测试环境：/tmp/test_ci_pollution.sh"
echo "3. 编译测试：/tmp/test_iptables_compile.sh"
echo "4. 完整构建：make K2P"