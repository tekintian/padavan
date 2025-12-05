# Padavan 自定义模块开发构建手册

## 前言

本手册以 SNI（Server Name Indication）自定义扩展为例，详细说明在 Padavan 固件构建系统中开发自定义模块的完整流程。SNI 模块是一个 iptables 扩展，用于根据 TLS/SSL 握手中的 SNI 域名进行网络流量匹配。

## 1. 系统架构概述

### 1.1 构建系统层次结构

```
Padavan 构建系统
├── 顶层控制 (trunk/Makefile)
├── 构建规则 (trunk/rules.mk)
├── 包模板 (trunk/include/package.mk)
└── 用户程序 (trunk/user/)
    ├── iptables/
    │   └── iptables-1.8.7/
    │       └── extensions/
    │           ├── GNUmakefile.in (自动发现机制)
    │           ├── libxt_sni.c (用户空间实现)
    │           └── ...
└── 内核模块 (trunk/linux-4.4.x/)
    └── net/netfilter/
        └── xt_sni.c (内核空间实现)
```

### 1.2 关键构建文件说明

#### 1.2.1 trunk/Makefile
- 顶层构建控制器，定义 `DIRS = libc libs user`
- 包含 `%_only:` 规则，支持目录级构建
- 控制完整的构建流程：`all: dep tools unpack_trx libc_only libs_only user_only romfs image`

#### 1.2.2 trunk/rules.mk
- 定义全局构建变量和工具链配置
- 设置交叉编译器路径和标志
- 定义安装工具和目标目录

#### 1.2.3 trunk/include/package.mk
- 提供 `BuildPackage` 模板
- 定义标准的准备、配置、编译、安装流程
- 支持 autotools 和 Makefile 两种构建模式

## 2. 自定义模块开发流程

### 2.1 模块类型确定

SNI 模块属于 iptables 扩展模块，需要：
- 内核空间实现：`net/netfilter/xt_sni.c`
- 用户空间实现：`user/iptables/iptables-1.8.7/extensions/libxt_sni.c`
- 头文件定义：`linux-4.4.x/include/uapi/linux/netfilter/xt_sni.h`

### 2.2 头文件开发

#### 2.2.1 内核头文件位置
```bash
# 文件路径
trunk/linux-4.4.x/include/uapi/linux/netfilter/xt_sni.h
```

#### 2.2.2 头文件导出配置
```bash
# 文件路径
trunk/linux-4.4.x/include/uapi/linux/netfilter/Kbuild

# 添加一行
header-y += xt_sni.h
```

#### 2.2.3 SNI 头文件示例
```c
#ifndef _XT_SNI_H
#define _XT_SNI_H

#include <linux/types.h>

#define XT_SNI_MAX_PATTERN_SIZE 128
#define XT_SNI_MAX_ALGO_NAME_SIZE 16

enum {
    XT_SNI_FLAG_INVERT     = 0x01,
    XT_SNI_FLAG_IGNORECASE = 0x02
};

struct xt_sni_info {
    __u16 from_offset;
    __u16 to_offset;
    char   algo[XT_SNI_MAX_ALGO_NAME_SIZE];
    char   pattern[XT_SNI_MAX_PATTERN_SIZE];
    __u8  patlen;
    union {
        struct {
            __u8  invert;
        } v0;
        struct {
            __u8  flags;
        } v1;
    } u;
    
    /* 内部使用 */
    struct ts_config __attribute__((aligned(8))) *config;
};

#endif /*_XT_SNI_H*/
```

### 2.3 内核模块开发

#### 2.3.1 内核模块位置
```bash
# 文件路径
trunk/linux-4.4.x/net/netfilter/xt_sni.c
```

#### 2.3.2 内核模块关键要素
```c
#include <linux/module.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_sni.h>
#include <linux/textsearch.h>

MODULE_AUTHOR("SNI Module");
MODULE_DESCRIPTION("Xtables: SNI-based matching");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_sni");
MODULE_ALIAS("ip6t_sni");

// 匹配函数
static bool sni_mt(const struct sk_buff *skb, struct xt_action_param *par);

// 检查函数
static int sni_mt_check(const struct xt_mtchk_param *par);

// 销毁函数
static void sni_mt_destroy(const struct xt_mtdtor_param *par);

// 注册结构体
static struct xt_match xt_sni_mt_reg __read_mostly = {
    .name       = "sni",
    .revision   = 1,
    .family     = NFPROTO_UNSPEC,
    .checkentry = sni_mt_check,
    .match      = sni_mt,
    .destroy    = sni_mt_destroy,
    .matchsize  = sizeof(struct xt_sni_info),
    .me         = THIS_MODULE,
};

// 模块初始化
module_init(sni_mt_init);
module_exit(sni_mt_exit);
```

### 2.4 用户空间扩展开发

#### 2.4.1 用户空间扩展位置
```bash
# 文件路径
trunk/user/iptables/iptables-1.8.7/extensions/libxt_sni.c
```

#### 2.4.2 用户空间扩展关键要素
```c
#include <xtables.h>
#include <linux/netfilter/xt_sni.h>

// 帮助信息
static void sni_help(void);

// 选项定义
static const struct xt_option_entry sni_opts[] = {
    {.name = "sni", .id = O_STRING, .type = XTTYPE_STRING,
     .flags = XTOPT_INVERT, .excl = F_HEX_STRING},
    {.name = "algo", .id = O_ALGO, .type = XTTYPE_STRING,
     .flags = XTOPT_MAND | XTOPT_PUT, XTOPT_POINTER(s, algo)},
    XTOPT_TABLEEND,
};

// 解析函数
static void sni_parse(struct xt_option_call *cb);

// 打印函数
static void sni_print(const void *ip, const struct xt_entry_match *match, int numeric);

// 保存函数
static void sni_save(const void *ip, const struct xt_entry_match *match);

// 注册结构体
static struct xtables_match sni_mt_reg[] = {
    {
        .name          = "sni",
        .revision      = 1,
        .family        = NFPROTO_UNSPEC,
        .version       = XTABLES_VERSION,
        .size          = XT_ALIGN(sizeof(struct xt_sni_info)),
        .userspacesize = offsetof(struct xt_sni_info, config),
        .help          = sni_help,
        .init          = sni_init,
        .print         = sni_print,
        .save          = sni_save,
        .x6_parse      = sni_parse,
        .x6_fcheck     = sni_check,
        .x6_options    = sni_opts,
    },
};

// 初始化函数
void _init(void)
{
    xtables_register_matches(sni_mt_reg, sizeof(sni_mt_reg)/sizeof(struct xtables_match));
}
```

## 3. 自动发现机制

### 3.1 iptables 扩展自动发现

iptables 构建系统使用自动发现机制，无需手动添加编译规则：

#### 3.1.1 自动发现配置
```makefile
# 文件：trunk/user/iptables/iptables-1.8.7/extensions/GNUmakefile.in

# 关键行：自动发现所有 libxt_*.c 文件
pfx_build_mod := $(patsubst ${srcdir}/libxt_%.c,%,$(sort $(wildcard ${srcdir}/libxt_*.c)))

# 自动生成目标
pfx_solibs    := $(patsubst %,libxt_%.so,${pfx_build_mod})
```

#### 3.1.2 工作原理
1. **文件扫描**：`wildcard ${srcdir}/libxt_*.c` 扫描所有 `libxt_*.c` 文件
2. **名称提取**：`patsubst` 提取模块名（如 `sni`）
3. **目标生成**：自动生成 `libxt_sni.so` 构建目标
4. **链接创建**：生成动态库并安装到 `$(xtlibdir)`

### 3.2 构建流程自动化

```bash
# 构建命令
make user_only

# 内部流程
1. 进入 trunk/user/ 目录
2. 执行 iptables 子目录构建
3. 自动发现 libxt_sni.c
4. 编译为 libxt_sni.so
5. 安装到 build/romfs/usr/lib/xtables/
```

## 4. 构建配置集成

### 4.1 配置选项定义

#### 4.1.1 模板配置文件
```bash
# 文件：trunk/configs/templates/K2P.config
CONFIG_FIRMWARE_INCLUDE_SNI_FILTER=y
```

#### 4.1.2 配置文件位置
```
trunk/configs/templates/
├── K2P.config
├── K2P-USB.config
├── K2P-TINY.config
└── K2P-NANO.config
```

### 4.2 构建规则配置

#### 4.2.1 用户程序 Makefile
```makefile
# 文件：trunk/user/Makefile
# 注意：SNI 作为 iptables 扩展，不需要独立构建条目
# 错误的配置（已注释）：
# dir_$(CONFIG_FIRMWARE_INCLUDE_SNI_FILTER)		+= sni
```

#### 4.2.2 iptables Makefile 配置
```makefile
# 文件：trunk/user/iptables/Makefile

include $(ROOTDIR)/rules.mk

PKG_NAME:=iptables
PKG_VERSION:=1.8.7

PKG_FIXUP:=autoreconf
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk

SRC_DIR=./iptables-1.8.7

CONFIGURE_ARGS += \
    --enable-static \
    --disable-shared \
    --disable-nftables \
    --disable-connlabel \
    --with-xtlibdir=/usr/lib/xtables \
    --with-kernel=$(ROOTDIR)/$(LINUXDIR) \
    --with-xt-lock-name="/var/lock/xtables.lock"

$(eval $(call BuildPackage,iptables))

romfs:
    $(INSTALL_DIR) $(ROMFSDIR)/usr/lib/xtables
    $(INSTALL_DIR) $(ROMFSDIR)/bin
    $(INSTALL_BIN) $(PKG_BUILD_DIR)/iptables/xtables-legacy-multi $(ROMFSDIR)/bin/xtables-multi
    $(LN) xtables-multi $(ROMFSDIR)/bin/iptables
    # ... 其他安装规则
```

## 5. 完整构建验证

### 5.1 构建环境检查

```bash
# 检查工具链
which mipsel-linux-musl-gcc

# 检查配置
grep CONFIG_FIRMWARE_INCLUDE_SNI_FILTER .config
```

### 5.2 模块编译验证

```bash
# 构建 iptables（包含 SNI 扩展）
make user_only

# 检查编译结果
ls -la trunk/build/romfs/usr/lib/xtables/libxt_sni.so
```

### 5.3 内核模块验证

```bash
# 构建内核（包含 SNI 模块）
make linux

# 检查模块编译
find trunk/linux-4.4.x/ -name "xt_sni.o" -o -name "xt_sni.ko"
```

### 5.4 完整固件构建

```bash
# 完整构建
make clean
make

# 检查最终镜像
ls -la images/
```

## 6. 常见问题与解决方案

### 6.1 构建失败：找不到模块

**问题**：`make[4]: *** sni: No such file or directory. Stop.`

**原因**：重复配置，同时在 iptables 扩展和独立模块中构建

**解决方案**：
```makefile
# 在 trunk/user/Makefile 中注释掉独立构建条目
# dir_$(CONFIG_FIRMWARE_INCLUDE_SNI_FILTER)		+= sni
```

### 6.2 头文件找不到

**问题**：编译时找不到 `xt_sni.h`

**原因**：未在 Kbuild 中导出头文件

**解决方案**：
```makefile
# 在 linux-4.4.x/include/uapi/linux/netfilter/Kbuild 中添加
header-y += xt_sni.h
```

### 6.3 模块注册失败

**问题**：模块无法注册到 iptables

**原因**：用户空间扩展初始化函数不正确

**解决方案**：
```c
// 确保初始化函数正确
void _init(void)
{
    xtables_register_matches(sni_mt_reg, sizeof(sni_mt_reg)/sizeof(struct xtables_match));
}
```

## 7. 最佳实践

### 7.1 开发规范

1. **命名规范**：遵循 `libxt_<module>.c` 和 `xt_<module>.c` 的命名模式
2. **头文件位置**：统一使用 `linux-4.4.x/include/uapi/linux/netfilter/`
3. **版本控制**：使用 revision 字段管理模块版本
4. **错误处理**：完善的参数检查和错误处理机制

### 7.2 测试策略

1. **单元测试**：分别测试内核和用户空间实现
2. **集成测试**：验证 iptables 规则加载和匹配
3. **性能测试**：测试大流量下的匹配性能
4. **兼容性测试**：验证不同内核版本的兼容性

### 7.3 维护建议

1. **文档同步**：及时更新模块文档和使用说明
2. **配置管理**：统一管理配置模板和默认选项
3. **版本发布**：建立版本标记和发布流程
4. **社区贡献**：考虑将成熟模块贡献给上游项目

## 8. Docker 模拟环境测试

### 8.1 Docker 环境概述

Padavan 提供了完整的 Docker 编译环境，支持模块的隔离测试和 CI/CD 集成。Docker 环境包含：

- **工具链**：mipsel-linux-musl-4.4.x 交叉编译器
- **依赖库**：完整的构建依赖和系统库
- **测试环境**：与目标环境一致的模拟测试

### 8.2 Docker 环境准备

#### 8.2.1 构建 Docker 镜像
```bash
# 基础编译环境镜像
docker build --build-arg APT_MIRROR_HOST=mirrors.tuna.tsinghua.edu.cn \
    -t tekintian/padavan-compiler:4.4.198 .

# CI 模拟环境镜像
docker build -f Dockerfile.ci-sim -t padavan-ci-sim .
```

#### 8.2.2 Docker 容器启动
```bash
# 交互式开发容器
docker run -itd --name padavan \
    -v /Volumes/csdisk/padavan:/padavan \
    tekintian/padavan-compiler:4.4.198

# 进入容器
docker exec -it padavan /bin/bash
```

### 8.3 模块构建测试

#### 8.3.1 环境验证测试
```bash
#!/bin/bash
# test_build_env.sh - 环境验证脚本

set -euo pipefail

# 颜色输出定义
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

echo -e "${BLUE}=== Docker 环境构建测试 ===${NC}"

# 1. 构建 Docker 镜像
log_info "构建 CI 模拟镜像..."
if docker build -f Dockerfile.ci-sim -t padavan-ci-sim . 2>&1 | tee logs/docker_build.log; then
    log_success "Docker 镜像构建成功"
else
    log_error "Docker 镜像构建失败"
    exit 1
fi

# 2. 测试环境变量
log_info "测试编译器环境..."
docker run --rm -v $(pwd):/padavan padavan-ci-sim sh -c "
    which mipsel-linux-musl-gcc && \
    mipsel-linux-musl-gcc --version | head -1
" 2>&1 | tee logs/env_test.log
```

#### 8.3.2 SNI 模块专项测试
```bash
#!/bin/bash
# test_sni_docker.sh - SNI 模块 Docker 测试

echo "=== Testing SNI module compilation in Docker ==="

# 检查 Docker 环境
if ! docker ps >/dev/null 2>&1; then
    echo "Error: Docker is not running"
    exit 1
fi

# 运行 SNI 模块编译测试
docker run --rm -v "$(pwd):/padavan" \
    -w /padavan tekintian/padavan-compiler sh -c "
    set -e
    
    echo '=== 环境检查 ==='
    which mipsel-linux-musl-gcc
    mipsel-linux-musl-gcc --version | head -1
    
    echo '=== 构建系统集成测试 ==='
    cd /padavan/trunk/user
    
    # 检查 SNI 配置
    if grep -q 'CONFIG_FIRMWARE_INCLUDE_SNI_FILTER.*sni' Makefile; then
        echo '✅ SNI 模块已正确集成'
    else
        echo '❌ SNI 模块未找到'
        exit 1
    fi
    
    # 尝试编译
    echo '编译 SNI 模块...'
    if make sni_only; then
        echo '✅ SNI 模块编译成功'
        
        # 检查生成的文件
        if [ -f sni/libxt_sni.so ]; then
            echo '✅ libxt_sni.so 已生成'
            file sni/libxt_sni.so
            ls -la sni/libxt_sni.so
        else
            echo '❌ libxt_sni.so 未找到'
        fi
    else
        echo '❌ SNI 模块编译失败'
        exit 1
    fi
"
```

#### 8.3.3 集成测试脚本
```bash
#!/bin/bash
# test_final_integration.sh - 最终集成测试

echo "=== SNI模块最终集成测试 ==="

export ROOTDIR=/padavan
cd $ROOTDIR

echo "🔧 步骤1: 准备iptables构建环境..."
cd trunk/build/iptables-1.8.7

# 创建必要头文件
echo '#define XTABLES_VERSION "1.8.7"' > include/xtables-version.h

# 复制 SNI 扩展文件
echo "📁 步骤2: 复制SNI扩展文件..."
cp /padavan/trunk/user/sni/libxt_sni.c extensions/
cp /padavan/trunk/linux-4.4.x/include/uapi/linux/netfilter/xt_sni.h include/linux/netfilter/

echo "🔨 步骤3: 编译SNI扩展..."
cd extensions
gcc -shared -fPIC \
    -I../include \
    -I../include/xtables \
    -I../../../linux-4.4.x/include/uapi \
    -I../../../linux-4.4.x/include \
    -o libxt_sni.so libxt_sni.c

if [ $? -eq 0 ]; then
    echo "✅ SNI扩展编译成功！"
else
    echo "❌ SNI扩展编译失败！"
    exit 1
fi

# 符号检查
echo "🧪 步骤4: 符号兼容性检查..."
cd .libs
nm -D libxt_sni.so | grep -E "(_init|_INIT)" && \
    echo "✅ 初始化符号正确" || \
    echo "⚠️  初始化符号异常"

# 模拟安装
echo "📦 步骤5: 模拟安装测试..."
mkdir -p /padavan/trunk/build/romfs/usr/lib/xtables
cp libxt_sni.so /padavan/trunk/build/romfs/usr/lib/xtables/

echo "🎉 SNI模块在Docker CI环境中编译成功！"
```

### 8.4 自动化测试流程

#### 8.4.1 完整测试管道
```bash
#!/bin/bash
# 完整的 Docker CI 测试管道

set -euo pipefail

# 创建日志目录
mkdir -p logs

echo "=== SNI 模块 Docker CI 测试管道 ==="

# 阶段1: 环境准备
echo "📦 阶段1: 环境准备..."
./test_build_env.sh | tee logs/01_environment.log

# 阶段2: 基础语法检查
echo "🔍 阶段2: 基础语法检查..."
./test_sni_compile.sh | tee logs/02_syntax.log

# 阶段3: Docker 编译测试
echo "🐳 阶段3: Docker 编译测试..."
./test_sni_docker.sh | tee logs/03_docker_build.log

# 阶段4: 集成测试
echo "🔗 阶段4: 集成测试..."
docker run --rm -v $(pwd):/padavan padavan-ci-sim \
    /tmp/test_final_integration.sh | tee logs/04_integration.log

# 生成测试报告
echo ""
echo "=== 测试报告 ==="
echo "📋 环境准备: $(grep -c 'SUCCESS' logs/01_environment.log) 项通过"
echo "📋 语法检查: $(grep -c 'OK\|PASS' logs/02_syntax.log) 项通过" 
echo "📋 Docker构建: $(grep -c '✅' logs/03_docker_build.log) 项通过"
echo "📋 集成测试: $(grep -c 'SUCCESS\|✅' logs/04_integration.log) 项通过"

echo ""
echo "🎉 Docker CI 测试管道执行完成！"
```

#### 8.4.2 调试和故障排除
```bash
# 进入调试容器
docker run -it --rm \
    -v $(pwd):/padavan \
    -w /padavan \
    tekintian/padavan-compiler:4.4.198 \
    /bin/bash

# 手动测试步骤
cd trunk/user
make clean
make sni_only

# 检查编译输出
ls -la sni/
file sni/libxt_sni.so

# 测试交叉编译
mipsel-linux-musl-gcc -v --version
```

### 8.5 CI/CD 集成

#### 8.5.1 GitHub Actions 配置示例
```yaml
name: Padavan Module CI

on: [push, pull_request]

jobs:
  test-module:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Build Docker environment
      run: |
        docker build -f Dockerfile.ci-sim -t padavan-ci-sim .
    
    - name: Test SNI module compilation
      run: |
        ./test_sni_docker.sh
    
    - name: Integration test
      run: |
        docker run --rm -v ${{ github.workspace }}:/padavan \
          padavan-ci-sim /tmp/test_final_integration.sh
```

#### 8.5.2 本地预提交检查
```bash
#!/bin/bash
# pre-commit-check.sh - 提交前检查

echo "=== 预提交检查 ==="

# 运行所有测试
./test_build_env.sh && \
./test_sni_compile.sh && \
./test_sni_docker.sh

if [ $? -eq 0 ]; then
    echo "✅ 所有检查通过，可以提交"
    exit 0
else
    echo "❌ 检查失败，请修复后重试"
    exit 1
fi
```

### 8.6 最佳实践

#### 8.6.1 Docker 环境优化
1. **缓存利用**：使用 ccache 加速编译
2. **网络优化**：配置国内镜像源
3. **存储优化**：使用 .dockerignore 减少上下文

#### 8.6.2 测试策略
1. **分层测试**：语法 → 编译 → 集成 → 功能
2. **并行执行**：独立测试并行运行
3. **失败快速**：失败时立即停止，节省资源

#### 8.6.3 持续改进
1. **日志收集**：详细记录测试过程
2. **性能监控**：跟踪构建时间趋势
3. **自动化报告**：生成测试结果报告

## 9. 总结

通过 SNI 模块的完整开发流程，我们了解了 Padavan 构建系统的核心机制：

1. **分层架构**：顶层控制、构建规则、包模板的清晰分层
2. **自动发现**：iptables 扩展的自动化构建机制
3. **配置集成**：通过模板配置控制模块编译
4. **交叉编译**：完整的嵌入式交叉编译支持
5. **Docker 测试**：隔离的开发和测试环境

遵循本手册的指导，可以高效地开发各种自定义模块，扩展 Padavan 固件的功能。关键在于理解构建系统的工作原理，遵循现有的开发规范，并充分利用自动发现和 Docker 测试等机制来简化开发和验证过程。