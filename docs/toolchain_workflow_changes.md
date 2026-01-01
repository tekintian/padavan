# 工具链构建 Workflow 优化说明

## 优化内容

### 1. 使用矩阵构建（Matrix Build）
- **之前**：为每个 libc（uclibc、musl）单独创建 job，代码重复
- **之后**：使用 GitHub Actions 的 strategy.matrix，自动构建 4 个变体：
  - x86_64 + uclibc
  - x86_64 + musl
  - arm64 + uclibc
  - arm64 + musl

### 2. 智能检测 Homebrew 前缀
- **问题**：混用了 `/usr/local/opt`（Intel Homebrew）和 `/opt/homebrew/opt`（Apple Silicon Homebrew）
- **解决**：动态检测运行环境架构，自动选择正确的 Homebrew 前缀
  ```bash
  if [[ "$(uname -m)" == "arm64" ]]; then
    HOMEBREW_PREFIX="/opt/homebrew"  # Apple Silicon
  else
    HOMEBREW_PREFIX="/usr/local"       # Intel
  fi
  ```

### 3. 修复 macOS sed 兼容性问题
- **问题**：macOS 的 sed 不支持 `-i` 参数
- **解决**：统一使用 `sed -i ''` 语法，兼容 macOS

### 4. 架构特定的部署目标
- **Intel (x86_64)**：
  - `MACOSX_DEPLOYMENT_TARGET=10.15` (macOS Catalina)
  - 工具链最低支持 macOS 10.15

- **Apple Silicon (arm64)**：
  - `MACOSX_DEPLOYMENT_TARGET=11.0` (macOS Big Sur)
  - 工具链最低支持 macOS 11.0

### 5. 优化文件命名
- **之前**：`mipsel-linux-uclibc-darwin`（无架构标识）
- **之后**：`mipsel-linux-uclibc-x86_64-darwin` 或 `mipsel-linux-uclibc-arm64-darwin`
  - 清晰区分 Intel 和 Apple Silicon 版本
  - 便于用户选择正确的版本

### 6. 改进 Release 说明
- 添加详细的版本说明和使用指南
- 清晰列出最低 macOS 版本要求
- 提供安装和使用示例

## 构建输出

### Linux 工具链（2个）
- `mipsel-linux-uclibc-linux-v1.26.0.tar.xz`
- `mipsel-linux-musl-linux-v1.26.0.tar.xz`

### macOS 工具链（4个）

#### Intel (x86_64) - 最低 macOS 10.15
- `mipsel-linux-uclibc-x86_64-darwin-v1.26.0.tar.xz`
- `mipsel-linux-musl-x86_64-darwin-v1.26.0.tar.xz`

#### Apple Silicon (arm64) - 最低 macOS 11.0
- `mipsel-linux-uclibc-arm64-darwin-v1.26.0.tar.xz`
- `mipsel-linux-musl-arm64-darwin-v1.26.0.tar.xz`

## 运行环境要求

### GitHub Actions Runner
- **macOS**: macos-14 (Sonoma) 或更高版本
- **Linux**: ubuntu-22.04

### 工具链最低系统要求
- **Intel x86_64**: macOS 10.15 (Catalina) 或更高
- **Apple Silicon arm64**: macOS 11.0 (Big Sur) 或更高

## 使用方法

### 下载对应架构的工具链
```bash
# Intel Mac 用户
wget https://github.com/your-repo/releases/download/v1.26.0/mipsel-linux-uclibc-x86_64-darwin-v1.26.0.tar.xz

# Apple Silicon Mac 用户
wget https://github.com/your-repo/releases/download/v1.26.0/mipsel-linux-uclibc-arm64-darwin-v1.26.0.tar.xz
```

### 解压并配置环境
```bash
sudo mkdir -p /opt/toolchain
sudo tar xJf toolchain-*.tar.xz -C /opt/toolchain
export PATH=/opt/toolchain/bin:$PATH

# 验证安装
mipsel-linux-gcc --version
```

## 技术细节

### 矩阵配置
```yaml
strategy:
  matrix:
    arch: [x86_64, arm64]
    libc: [uclibc, musl]
```
这会自动生成 4 个并行构建任务：
1. arch=x86_64, libc=uclibc
2. arch=x86_64, libc=musl
3. arch=arm64, libc=uclibc
4. arch=arm64, libc=musl

### 环境变量传递
- 使用 `$GITHUB_PATH` 设置 PATH（仅当前 job 有效）
- 使用 `$GITHUB_ENV` 设置环境变量（跨 step 有效）
- 矩阵变量通过 `${{ matrix.arch }}` 和 `${{ matrix.libc }}` 访问

### 交叉编译注意事项
- 工具链本身是在 macOS 上为 mipsel 架构构建的
- macOS 的部署目标仅影响工具链本身的兼容性
- 不影响目标 mipsel 程序的运行
