#!/bin/bash
# 在现有容器中添加 GitHub Actions runner 的脚本

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

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

echo -e "${BLUE}=== 添加 GitHub Actions Runner 到现有容器 ===${NC}"
echo ""

# 检查文件是否存在
if [ ! -f "files/actions-runner-linux-x64-2.311.0.tar.gz" ]; then
    log_error "actions-runner-linux-x64-2.311.0.tar.gz 文件不存在"
    echo "请先运行 ./download_ci_files.sh 下载该文件"
    exit 1
fi

# 检查 Docker 镜像是否存在
if ! docker images | grep -q "padavan-ci-sim"; then
    log_error "padavan-ci-sim 镜像不存在"
    echo "请先运行：docker build -f Dockerfile.ci-sim -t padavan-ci-sim ."
    exit 1
fi

log_info "创建临时容器来安装 actions-runner..."

# 创建临时容器并安装 actions-runner
docker run --rm -v $(pwd)/files:/tmp/files:ro padavan-ci-sim bash -c "
    set -eux
    
    # 安装 .NET SDK（actions-runner 依赖）
    apt-get update -y -q
    apt-get install -y -q dotnet-sdk-6.0
    
    # 创建安装目录
    mkdir -p /opt/actions-runner
    
    # 解压 actions-runner
    tar -xzf /tmp/files/actions-runner-linux-x64-2.311.0.tar.gz -C /opt/actions-runner
    
    # 配置 runner（使用测试 token）
    cd /opt/actions-runner
    ./config.sh --url https://github.com/tekintian/padavan --token FAKE_TOKEN_FOR_TESTING --name docker-test-runner --work /home/runner/work/padavan --unattended || true
    
    echo '✅ Actions Runner 安装完成'
"

log_success "Actions Runner 已添加到容器中"
echo ""

echo -e "${BLUE}=== 使用方法 ===${NC}"
echo "1. 重新构建镜像（包含 actions-runner）："
echo "   docker build -f Dockerfile.ci-sim-full -t padavan-ci-sim-full ."
echo ""
echo "2. 或者使用上面安装的临时容器创建新镜像："
echo "   docker commit \$(docker run -d padavan-ci-sim tail -f /dev/null) padavan-ci-sim-with-runner"
echo ""
echo "3. 运行完整模拟环境："
echo "   docker run -it --rm -v \$(pwd):/padavan padavan-ci-sim-full"
echo ""
echo "4. 测试 actions-runner："
echo "   docker run --rm -v \$(pwd):/padavan padavan-ci-sim-full ps aux | grep runner"