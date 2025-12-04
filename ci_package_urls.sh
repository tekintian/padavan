#!/bin/bash
# GitHub Actions CI 环境所需软件包下载地址列表
# 使用方法：手动下载这些文件后放到 files/ 目录，或使用 download_ci_files.sh 自动下载

set -euo pipefail

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 创建本地文件目录
LOCAL_FILES_DIR="$(pwd)/files"
mkdir -p "${LOCAL_FILES_DIR}"

echo -e "${BLUE}=== GitHub Actions CI 环境软件包下载列表 ===${NC}"
echo -e "${YELLOW}本地文件目录：${LOCAL_FILES_DIR}${NC}"
echo ""

# 1. 工具链 (必需)
echo -e "${GREEN}1. MIPSel 工具链 (必需)${NC}"
echo "   文件名：mipsel-linux-musl_4.4.x.tar.xz"
echo "   下载地址："
echo "   - 主地址：https://github.com/tekintian/padavan/releases/download/toolchain/mipsel-linux-musl.tar.xz"
echo "   - 备用地址：https://cdn.jsdelivr.net/gh/tekintian/padavan@toolchain/mipsel-linux-musl.tar.xz"
echo "   - uclibc版本：https://github.com/tekintian/padavan/releases/download/toolchain/mipsel-linux-uclibc.tar.xz"
echo "   大小：~30MB"
echo "   用途：MIPSel 架构交叉编译工具链（tekintian/padavan项目）"
echo ""

# 2. Go 语言环境 (GitHub Actions setup-go@v4)
echo -e "${GREEN}2. Go 语言环境 (GitHub Actions 要求)${NC}"
echo "   文件名：go1.20.14.linux-amd64.tar.gz"
echo "   下载地址："
echo "   - 官方地址：https://go.dev/dl/go1.20.14.linux-amd64.tar.gz"
echo "   - 备用地址：https://golang.org/dl/go1.20.14.linux-amd64.tar.gz"
echo "   - 镜像地址：https://mirrors.ustc.edu.cn/golang/go1.20.14.linux-amd64.tar.gz"
echo "   大小：~98MB"
echo "   用途：GitHub Actions setup-go@v4 要求的 Go 1.20 版本"
echo ""

# 3. Node.js 环境 (GitHub Actions setup-node@v3)
echo -e "${GREEN}3. Node.js 环境 (GitHub Actions 要求)${NC}"
echo "   文件名：node-v18.19.0-linux-x64.tar.xz"
echo "   下载地址："
echo "   - 官方地址：https://nodejs.org/dist/v18.19.0/node-v18.19.0-linux-x64.tar.xz"
echo "   - 备用地址：https://nodejs.mirrors.ustc.edu.cn/dist/v18.19.0/node-v18.19.0-linux-x64.tar.xz"
echo "   - 镜像地址：https://mirrors.tuna.tsinghua.edu.cn/nodejs-release/v18.19.0/node-v18.19.0-linux-x64.tar.xz"
echo "   大小：~24MB"
echo "   用途：GitHub Actions setup-node@v3 要求的 Node.js 18 版本"
echo ""

# 4. ccache action 相关文件 (可选，用于模拟 ccache-action)
echo -e "${GREEN}4. ccache action 文件 (可选，用于完整模拟)${NC}"
echo "   文件名：ccache-action-v1.2.tar.gz"
echo "   下载地址："
echo "   - GitHub 地址：https://github.com/hendrikmuhs/ccache-action/archive/refs/tags/v1.2.tar.gz"
echo "   - 备用地址：https://cdn.jsdelivr.net/gh/hendrikmuhs/ccache-action@v1.2/ccache-action.tar.gz"
echo "   大小：~50KB"
echo "   用途：模拟 GitHub Actions ccache-action，创建相同的环境变量"
echo ""

# 5. GitHub Actions runner 脚本 (可选)
echo -e "${GREEN}5. GitHub Actions runner 脚本 (可选)${NC}"
echo "   文件名：actions-runner-linux-x64-2.311.0.tar.gz"
echo "   下载地址："
echo "   - GitHub 地址：https://github.com/actions/runner/releases/download/v2.311.0/actions-runner-linux-x64-2.311.0.tar.gz"
echo "   - 备用地址：https://cdn.jsdelivr.net/gh/actions/runner@v2.311.0/actions-runner-linux-x64-2.311.0.tar.gz"
echo "   大小：~40MB"
echo "   用途：模拟完整的 GitHub Actions runner 环境"
echo ""

echo -e "${BLUE}=== 使用说明 ===${NC}"
echo -e "${YELLOW}方法一：手动下载${NC}"
echo "1. 手动下载上述文件"
echo "2. 将文件放到 ${LOCAL_FILES_DIR}/ 目录"
echo "3. 运行: docker build -f Dockerfile.ci-sim -t padavan-ci-sim ."
echo ""

echo -e "${YELLOW}方法二：自动下载${NC}"
echo "运行: ./download_ci_files.sh"
echo "脚本会自动检查本地文件，如果不存在则从网络下载"
echo ""

echo -e "${YELLOW}方法三：混合模式${NC}"
echo "1. 部分文件手动下载（如工具链）"
echo "2. 运行: ./download_ci_files.sh"
echo "3. 脚本会跳过已存在的文件，只下载缺失的文件"
echo ""

echo -e "${BLUE}=== 文件检查 ===${NC}"
echo "检查本地 files/ 目录中的文件："

# 检查必需文件
required_files=(
    "mipsel-linux-musl_4.4.x.tar.xz"
    "go1.20.14.linux-amd64.tar.gz"
    "node-v18.19.0-linux-x64.tar.xz"
)

optional_files=(
    "ccache-action-v1.2.tar.gz"
    "actions-runner-linux-x64-2.311.0.tar.gz"
)

echo -e "\n${GREEN}必需文件：${NC}"
for file in "${required_files[@]}"; do
    if [ -f "${LOCAL_FILES_DIR}/${file}" ]; then
        size=$(ls -lh "${LOCAL_FILES_DIR}/${file}" | awk '{print $5}')
        echo -e "  ✅ ${file} (${size})"
    else
        echo -e "  ❌ ${file} (缺失)"
    fi
done

echo -e "\n${YELLOW}可选文件：${NC}"
for file in "${optional_files[@]}"; do
    if [ -f "${LOCAL_FILES_DIR}/${file}" ]; then
        size=$(ls -lh "${LOCAL_FILES_DIR}/${file}" | awk '{print $5}')
        echo -e "  ✅ ${file} (${size})"
    else
        echo -e "  ⚪ ${file} (未提供)"
    fi
done

echo ""
echo -e "${BLUE}=== 快速下载命令 ===${NC}"
echo "# 使用 wget 下载必需文件"
echo "cd ${LOCAL_FILES_DIR}"
echo "wget https://github.com/tekintian/padavan/releases/download/toolchain/mipsel-linux-musl_4.4.x.tar.xz"
echo "wget https://go.dev/dl/go1.20.14.linux-amd64.tar.gz"
echo "wget https://nodejs.org/dist/v18.19.0/node-v18.19.0-linux-x64.tar.xz"
echo ""
echo "# 或使用 curl 下载"
echo "cd ${LOCAL_FILES_DIR}"
echo "curl -L -O https://github.com/tekintian/padavan/releases/download/toolchain/mipsel-linux-musl_4.4.x.tar.xz"
echo "curl -L -O https://go.dev/dl/go1.20.14.linux-amd64.tar.gz"
echo "curl -L -O https://nodejs.org/dist/v18.19.0/node-v18.19.0-linux-x64.tar.xz"
echo ""