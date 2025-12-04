#!/bin/bash
# GitHub Actions CI 环境软件包下载脚本
# 支持本地文件优先，网络下载备用

set -euo pipefail

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 配置
LOCAL_FILES_DIR="$(pwd)/files"
TEMP_DIR="/tmp/padavan-ci-$$"
USER_AGENT="Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"

# 创建目录
mkdir -p "${LOCAL_FILES_DIR}"
mkdir -p "${TEMP_DIR}"

# 清理函数
cleanup() {
    rm -rf "${TEMP_DIR}"
}
trap cleanup EXIT

# 日志函数
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

# 检查文件是否存在且完整
check_file() {
    local file="$1"
    local expected_size="$2"
    
    if [ ! -f "${file}" ]; then
        return 1
    fi
    
    # 检查文件大小（如果提供了期望大小）
    if [ -n "${expected_size}" ]; then
        local actual_size=$(stat -f%z "${file}" 2>/dev/null || stat -c%s "${file}" 2>/dev/null || echo "0")
        if [ "${actual_size}" -lt 1000000 ]; then  # 小于1MB可能是不完整文件
            log_warning "${file} 文件过小，可能下载不完整"
            return 1
        fi
    fi
    
    return 0
}

# 下载函数（支持多个URL）
download_file() {
    local filename="$1"
    local expected_size="$2"
    shift 2
    local urls=("$@")
    
    local target_file="${LOCAL_FILES_DIR}/${filename}"
    
    # 检查本地文件是否已存在且完整
    if check_file "${target_file}" "${expected_size}"; then
        local size=$(ls -lh "${target_file}" | awk '{print $5}')
        log_success "${filename} 已存在 (${size})，跳过下载"
        return 0
    fi
    
    log_info "开始下载 ${filename}..."
    
    # 尝试每个URL
    for url in "${urls[@]}"; do
        log_info "尝试从 ${url} 下载..."
        
        # 使用 curl 下载（支持重定向）
        if curl --fail --location --user-agent "${USER_AGENT}" --retry 3 --retry-delay 2 \
                --output "${target_file}.tmp" "${url}" 2>/dev/null; then
            
            # 检查下载的文件
            if check_file "${target_file}.tmp" "${expected_size}"; then
                mv "${target_file}.tmp" "${target_file}"
                local size=$(ls -lh "${target_file}" | awk '{print $5}')
                log_success "${filename} 下载完成 (${size})"
                return 0
            else
                log_warning "下载的文件不完整，尝试下一个URL"
                rm -f "${target_file}.tmp"
            fi
        else
            log_warning "从 ${url} 下载失败，尝试下一个URL"
            rm -f "${target_file}.tmp"
        fi
    done
    
    log_error "${filename} 所有下载尝试都失败了"
    return 1
}

# 显示下载进度
show_progress() {
    local current=$1
    local total=$2
    local filename="$3"
    
    local percent=$((current * 100 / total))
    local bar_length=30
    local filled_length=$((percent * bar_length / 100))
    
    printf "\r${BLUE}[PROGRESS]${NC} ["
    printf "%*s" ${filled_length} | tr ' ' '='
    printf "%*s" $((bar_length - filled_length)) | tr ' ' '-'
    printf "] %d%% (%s)" ${percent} "${filename}"
}

echo -e "${BLUE}=== GitHub Actions CI 环境软件包下载器 ===${NC}"
echo -e "${YELLOW}本地文件目录：${LOCAL_FILES_DIR}${NC}"
echo -e "${YELLOW}临时目录：${TEMP_DIR}${NC}"
echo ""

# 定义下载列表 (文件名, 期望大小(字节), URL列表)
declare -a downloads=(
    "mipsel-linux-musl_4.4.x.tar.xz|31457280|\
https://github.com/tekintian/padavan/releases/download/toolchain/mipsel-linux-musl_4.4.x.tar.xz|\
https://cdn.jsdelivr.net/gh/tekintian/padavan@toolchain/mipsel-linux-musl_4.4.x.tar.xz|\
https://github.com/tekintian/padavan/releases/download/toolchain/mipsel-linux-uclibc_4.4.x.tar.xz"

    "go1.20.14.linux-amd64.tar.gz|102760000|\
https://go.dev/dl/go1.20.14.linux-amd64.tar.gz|\
https://golang.org/dl/go1.20.14.linux-amd64.tar.gz|\
https://mirrors.ustc.edu.cn/golang/go1.20.14.linux-amd64.tar.gz"

    "node-v18.19.0-linux-x64.tar.xz|25165824|\
https://nodejs.org/dist/v18.19.0/node-v18.19.0-linux-x64.tar.xz|\
https://nodejs.mirrors.ustc.edu.cn/dist/v18.19.0/node-v18.19.0-linux-x64.tar.xz|\
https://mirrors.tuna.tsinghua.edu.cn/nodejs-release/v18.19.0/node-v18.19.0-linux-x64.tar.xz"

    "ccache-action-v1.2.tar.gz|51200|\
https://github.com/hendrikmuhs/ccache-action/archive/refs/tags/v1.2.tar.gz|\
https://cdn.jsdelivr.net/gh/hendrikmuhs/ccache-action@v1.2/ccache-action.tar.gz"

    "actions-runner-linux-x64-2.311.0.tar.gz|41943040|\
https://github.com/actions/runner/releases/download/v2.311.0/actions-runner-linux-x64-2.311.0.tar.gz|\
https://cdn.jsdelivr.net/gh/actions/runner@v2.311.0/actions-runner-linux-x64-2.311.0.tar.gz"
)

# 统计变量
total_files=${#downloads[@]}
success_count=0
failed_count=0

# 开始下载
log_info "开始下载 ${total_files} 个文件..."
echo ""

for i in "${!downloads[@]}"; do
    IFS='|' read -r filename expected_size urls <<< "${downloads[$i]}"
    
    # 显示进度
    show_progress $((i + 1)) ${total_files} "${filename}"
    
    # 解析URL数组
    IFS='|' read -ra url_array <<< "${urls}"
    
    # 下载文件
    if download_file "${filename}" "${expected_size}" "${url_array[@]}"; then
        ((success_count++))
    else
        ((failed_count++))
    fi
    
    echo ""
done

# 显示结果
echo ""
echo -e "${BLUE}=== 下载结果 ===${NC}"
echo -e "${GREEN}成功：${success_count} 个文件${NC}"
echo -e "${RED}失败：${failed_count} 个文件${NC}"

if [ ${failed_count} -gt 0 ]; then
    echo ""
    log_warning "部分文件下载失败，您可以："
    echo "1. 检查网络连接"
    echo "2. 手动下载失败的文件"
    echo "3. 运行 ./ci_package_urls.sh 查看详细的下载地址"
    echo "4. 使用代理或VPN"
fi

echo ""
echo -e "${BLUE}=== 文件验证 ===${NC}"

# 验证必需文件
required_files=(
    "mipsel-linux-musl_4.4.x.tar.xz"
    "go1.20.14.linux-amd64.tar.gz"
    "node-v18.19.0-linux-x64.tar.xz"
)

all_required_exist=true
for file in "${required_files[@]}"; do
    if [ -f "${LOCAL_FILES_DIR}/${file}" ]; then
        size=$(ls -lh "${LOCAL_FILES_DIR}/${file}" | awk '{print $5}')
        echo -e "  ✅ ${file} (${size})"
    else
        echo -e "  ❌ ${file} (缺失)"
        all_required_exist=false
    fi
done

if ${all_required_exist}; then
    echo ""
    log_success "所有必需文件已准备就绪！"
    echo ""
    echo -e "${YELLOW}下一步：${NC}"
    echo "docker build -f Dockerfile.ci-sim -t padavan-ci-sim ."
else
    echo ""
    log_error "缺少必需文件，请确保所有必需文件都下载成功"
    exit 1
fi