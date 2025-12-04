#!/bin/bash
set -euo pipefail

# ======================================
# 自动获取脚本所在路径（核心优化）
# ======================================
SCRIPT_DIR=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
echo -e "ℹ️  脚本所在目录：${SCRIPT_DIR}"

# ======================================
# 解析命令行参数（关键修改：支持外部传递型号）
# ======================================
if [ $# -ne 1 ]; then
    echo "❌ 用法错误：请传递路由器型号作为参数（对应 configs/templates/[型号].config）"
    echo "示例："
    echo "  ./build_padavan.sh K2P        # 编译 K2P（配置文件：configs/templates/K2P.config）"
    echo "  ./build_padavan.sh ac54u      # 编译 RT-AC54U（配置文件：configs/templates/ac54u.config）"
    echo "  ./build_padavan.sh mi-mini    # 编译小米 Mini（配置文件：configs/templates/mi-mini.config）"
    exit 1
fi

# 从参数获取路由器型号，自动拼接配置文件路径
ROUTER_MODEL="$1"
CONFIG_FILE="${ROUTER_MODEL}.config"  # 自动拼接为：型号.config
CONFIG_PATH="${SCRIPT_DIR}/configs/templates/${CONFIG_FILE}"  # 完整配置路径

# ======================================
# 其他配置（无需修改）
# ======================================
THREADS=$(nproc)  # 自动识别 CPU 核心数
BACKUP_DIR="${SCRIPT_DIR}/compile_output"  # 产物备份目录（脚本所在目录/compile_output）

# ======================================
# 核心编译流程
# ======================================
echo -e "\n======================================"
echo -e "📦 Padavan 固件一键编译脚本（参数化版）"
echo -e "🔧 目标路由器：${ROUTER_MODEL}"
echo -e "🔧 配置文件路径：${CONFIG_PATH}"
echo -e "🚀 编译线程数：${THREADS}（CPU 核心数）"
echo -e "💾 产物备份目录：${BACKUP_DIR}"
echo -e "======================================\n"

# 1. 检查源码目录完整性
if [ ! -d "${SCRIPT_DIR}/configs/templates" ] || [ ! -f "${SCRIPT_DIR}/build.sh" ]; then
    echo "❌ 错误：脚本所在目录不是 Padavan 源码根目录！"
    echo "当前脚本路径：${SCRIPT_DIR}"
    echo "要求：脚本需放在源码根目录（含 build.sh、configs 文件夹的目录）"
    exit 1
fi

# 2. 进入源码根目录
cd "${SCRIPT_DIR}"
echo -e "✅ 已进入源码根目录：${PWD}"

# 3. 检查配置文件是否存在（参数传递的型号是否有效）
if [ ! -f "${CONFIG_PATH}" ]; then
    echo "❌ 错误：未找到配置文件 ${CONFIG_PATH}"
    echo "可用配置文件列表（直接传递型号作为参数）："
    ls -l configs/templates/ | grep ".config" | awk '{print "  - " $9}' | sed 's/\.config//g'
    exit 1
fi

# 4. 检查编译器是否可用
if ! command -v mipsel-linux-musl-gcc &> /dev/null; then
    echo "⚠️  编译器未全局识别，尝试手动加载 PATH..."
    export PATH="/usr/local/mipsel-toolchain-4.4.x/bin:/padavan/toolchain/toolchain-mipsel/toolchain-4.4.x/bin:$PATH"
    if ! command -v mipsel-linux-musl-gcc &> /dev/null; then
        echo "❌ 错误：编译器加载失败！"
        exit 1
    fi
fi
echo -e "✅ 编译器验证通过：$(mipsel-linux-musl-gcc --version | head -n1 | awk '{print $1,$2,$3}')"

# 5. 清理上次编译残留
echo -e "\n🧹 清理上次编译残留..."
make clean &> /dev/null
rm -rf bin/  # 清除旧固件目录
rm -f compile_log.txt  # 清除旧日志

# 6. 加载路由器配置（使用参数传递的型号对应的配置文件）
echo -e "⚙️  加载配置文件：${CONFIG_FILE}..."
cp -f "${CONFIG_PATH}" .config  # 复制配置文件到源码根目录
./build.sh clean &> /dev/null  # 适配 Padavan 源码的清理命令

# 7. 开始编译（多线程加速）
echo -e "🚀 开始编译 ${ROUTER_MODEL}（${THREADS} 线程）... 预计耗时 30-60 分钟"
echo "日志输出：${SCRIPT_DIR}/compile_log.txt（实时查看：tail -f ${SCRIPT_DIR}/compile_log.txt）"
make -j${THREADS} 2>&1 | tee compile_log.txt  # 日志保存到脚本所在目录

# 8. 检查编译结果
if [ ! -d "bin" ] || [ -z "$(ls bin/*.trx 2>/dev/null)" ]; then
    echo -e "\n❌ 编译失败！查看日志：tail -f ${SCRIPT_DIR}/compile_log.txt"
    exit 1
fi

# 9. 备份编译产物
echo -e "\n💾 备份编译产物到 ${BACKUP_DIR}..."
mkdir -p ${BACKUP_DIR}
FIRMWARE_FILE=$(ls bin/*.trx | head -1)  # 获取固件文件（.trx 格式）
FIRMWARE_NAME=$(basename ${FIRMWARE_FILE})
# 备份固件（带型号+时间戳，避免覆盖不同型号的产物）
BACKUP_FIRMWARE="${BACKUP_DIR}/${ROUTER_MODEL}_${FIRMWARE_NAME}_$(date +%Y%m%d_%H%M%S).trx"
# 备份日志和配置文件（带型号标识）
BACKUP_LOG="${BACKUP_DIR}/${ROUTER_MODEL}_compile_log_$(date +%Y%m%d_%H%M%S).txt"
BACKUP_CONFIG="${BACKUP_DIR}/${ROUTER_MODEL}_config_$(date +%Y%m%d_%H%M%S).txt"

cp -f ${FIRMWARE_FILE} ${BACKUP_FIRMWARE}
cp -f compile_log.txt ${BACKUP_LOG}
cp -f .config ${BACKUP_CONFIG}

# 10. 输出成功信息
echo -e "\n======================================"
echo -e "🎉 ${ROUTER_MODEL} 固件编译成功！"
echo -e "📁 固件路径（容器内）：${BACKUP_FIRMWARE}"
echo -e "📁 本地路径（宿主机）：$(echo ${BACKUP_FIRMWARE} | sed 's/\/padavan\//\/你的本地源码路径\//g')"
echo -e "💡 刷机说明：使用路由器 Breed/Padavan 后台的「固件升级」功能上传 .trx 文件"
echo -e "======================================\n"