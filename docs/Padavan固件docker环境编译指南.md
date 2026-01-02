# Padavan固件docker环境编译指南

## 前言
本指南旨在指导用户如何在Docker容器中配置和使用Ubuntu环境来编译Padavan固件。通过Docker，可以提供一个标准化、可重复的构建环境，确保编译过程的一致性。

另外如果是macos默认是不区分大小写的, 如果要拉取项目,
需要先创建一个区分大小写的虚拟磁盘,然后在这个磁盘里面操作,详见 docs/macos创建区分大小写的虚拟磁盘.md

## Docker环境配置

### Docker镜像获取与配置
```bash
# 拉取Ubuntu 22.04镜像（使用镜像加速）
docker pull m.daocloud.io/docker.io/ubuntu:22.04

# 重命名镜像
docker tag m.daocloud.io/docker.io/ubuntu:22.04 ubuntu:22.04

# 删除原始镜像引用
docker rmi m.daocloud.io/docker.io/ubuntu:22.04

# 运行Docker容器并挂载Padavan源码目录
docker run -it --name padavan-build -v /Volumes/csdisk/padavan:/padavan ubuntu:22.04
```

### 容器内软件源加速配置
```bash
# 备份原始软件源配置
cp /etc/apt/sources.list /etc/apt/sources.list.bak

# 替换为阿里云镜像源
sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list
```

## 完整依赖包列表

根据GitHub Actions成功编译的日志和我们之前解决的编译问题，整理出以下完整的依赖包列表：

### 核心编译工具
- `build-essential` - 基础编译工具集（gcc、g++、make等）
- `ccache` - 编译缓存，加速重复编译
- `bc` - 数学计算工具，内核编译需要
- `flex` - 词法分析器生成器
- `bison` - 语法分析器生成器

### 开发库
- `zlib1g-dev` - zlib压缩库开发文件
- `libssl-dev` - OpenSSL开发库
- `libncurses5-dev` - 终端界面开发库

### 辅助工具
- `libtool-bin` - 库工具
- `gperf` - 完美哈希函数生成器
- `gettext` - 国际化工具
- `autopoint` - gettext相关工具

### Python工具
- `python3-docutils` - Python文档生成工具
- `python3-pil` - Python图像处理库
- `python3-olefile` - Python OLE文件处理库
- `python3-roman` - Python罗马数字处理库

### XML工具
- `sgml-base` - SGML基础包
- `xml-core` - XML核心包

## 完整安装命令

在Docker容器（Ubuntu 22.04）中执行以下命令安装所有依赖：

```bash
# 更新软件包列表
apt-get update

# 安装所有依赖包
apt-get install -y \
  build-essential \
  ccache \
  bc \
  flex \
  bison \
  zlib1g-dev \
  libssl-dev \
  libncurses5-dev \
  libtool-bin \
  gperf \
  gettext \
  autopoint \
  python3-docutils \
  python3-pil \
  python3-olefile \
  python3-roman \
  sgml-base \
  xml-core
```

## 编译前准备

1. **修复Kconfig递归依赖问题**（如之前发现的）：
   ```bash
   vi /padavan/trunk/linux-4.4.x/net/netfilter/Kconfig
   ```
   移除`NETFILTER_XT_MATCH_SNI`配置中的`select NETFILTER_XTABLES`行

2. **清理之前的编译状态**：
   ```bash
   make clean
   ```

## 执行编译

```bash
# 编译K2P固件，使用mipsel-linux-musl工具链
make K2P TOOLCHAIN=mipsel-linux-musl
```

## 编译时间说明

- 首次编译时间较长（可能需要1-3小时），因为需要编译大量组件
- 使用ccache后，后续重新编译会快很多

## 编译结果

编译成功后，固件文件将位于：
`/padava/trunk/images/` 目录中

按照以上步骤操作，应该能够成功在Docker容器中编译Padavan固件
