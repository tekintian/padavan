# Padavan系统中SNI模块增加详细指南

本文档详细总结了在Padavan固件中增加SNI（Server Name Indication）过滤模块的完整流程，包括从Web界面配置到后端实现，以及为iptables添加自定义扩展模块的通用方法。

## 一、SNI模块概述

SNI模块允许在TLS握手过程中解析ClientHello包中的Server Name Indication字段，从而实现对HTTPS流量的域名级过滤，无需解密HTTPS流量即可基于域名进行访问控制。

## 二、SNI模块实现与集成流程

### 1. 内核模块开发

#### 1.1 模块源码实现
SNI过滤模块的核心文件位于：
- `trunk/linux-4.4.x/net/netfilter/xt_sni_filter.c`（模块主要实现）

该模块实现了：
- SNI字符串的提取和匹配
- 与iptables框架的集成
- 匹配规则的处理逻辑

#### 1.2 模块编译配置

**Kconfig配置**（关键文件）：
- `trunk/linux-4.4.x/net/netfilter/Kconfig`

SNI模块在Kconfig中的定义如下：
```
config NETFILTER_XT_MATCH_SNI
    tristate "SNI match support"
    depends on NETFILTER_XTABLES
    help
      This option adds a `sni' match, which allows matching the
      SNI (Server Name Indication) from TLS ClientHello packets.
      #select NETFILTER_XTABLES  <-- 注意：这行必须注释，避免递归依赖
```

**Makefile配置**：
- `trunk/linux-4.4.x/net/netfilter/Makefile`

关键编译规则：
```
obj-$(CONFIG_NETFILTER_XT_MATCH_SNI) += xt_sni_filter.o
```

### 2. 内核配置启用

#### 2.1 设备特定配置
在K2P设备配置中启用SNI模块：
- `trunk/configs/boards/K2P/kernel-4.4.x.config`

确保包含以下配置：
```
CONFIG_NETFILTER_XTABLES=y
CONFIG_NETFILTER_XT_MATCH_SNI=y
```

#### 2.2 固件功能配置
在固件模板配置中启用SNI功能：
- `trunk/configs/templates/K2P.config`

添加功能开关：
```
CONFIG_FIRMWARE_INCLUDE_SNI_FILTER=y
```

### 3. 解决递归依赖问题

**问题根源**：SNI模块的Kconfig中错误地包含了`select NETFILTER_XTABLES`，而该模块本身又`depends on NETFILTER_XTABLES`，形成递归依赖。

**解决方案**：
1. 编辑Kconfig文件，注释或删除`select NETFILTER_XTABLES`行：
   ```bash
   sed -i 's/select NETFILTER_XTABLES/#select NETFILTER_XTABLES/g' trunk/linux-4.4.x/net/netfilter/Kconfig
   ```

2. 清理编译环境：
   ```bash
   make clean
   ```

3. 重新编译固件：
   ```bash
   make K2P TOOLCHAIN=mipsel-linux-musl
   ```

## 三、Web界面集成

### 1. 界面表单设计
SNI过滤规则通常集成在防火墙或URL过滤页面中，提供以下配置选项：
- 过滤模式：允许/拒绝
- 域名：要过滤的目标域名
- 适用设备：通过MAC地址指定特定设备
- 端口设置：通常针对443端口（HTTPS）

### 2. 后端处理流程

#### 2.1 配置处理
Web界面表单提交后，后端处理流程如下：

1. **配置解析**：将Web表单数据解析为规则配置
2. **规则生成**：根据配置生成对应的iptables规则
3. **规则应用**：将生成的规则添加到对应的iptables链中

#### 2.2 iptables规则示例
生成的SNI过滤规则示例：
```bash
# 拒绝特定MAC地址访问特定域名
iptables -A FORWARD -m mac --mac-source XX:XX:XX:XX:XX:XX -p tcp --dport 443 -m sni --sni-domain "example.com" -j REJECT

# 允许特定域名访问
iptables -A FORWARD -p tcp --dport 443 -m sni --sni-domain "trusted-site.com" -j ACCEPT
```

#### 2.3 配置持久化
规则配置需要持久化保存，通常存储在：
- `/etc/storage/` 目录下的配置文件
- 在设备重启时自动加载应用

## 四、在Padavan中为iptables增加自定义模块的通用方法

### 1. 准备工作

#### 1.1 环境设置
- 确保已安装交叉编译工具链：`toolchain/mipsel-linux-musl/`
- 熟悉内核编译环境和Padavan固件结构

### 2. 自定义模块开发步骤

#### 2.1 模块源码创建
1. 在`trunk/linux-4.4.x/net/netfilter/`目录创建模块源文件
   ```
   xt_yourmodule.c  # 模块主要实现
   xt_yourmodule.h  # 头文件定义（如需）
   ```

2. 实现必要的iptables钩子函数：
   - 匹配函数（match function）
   - 检查函数（check function）
   - 销毁函数（destroy function）
   - 帮助函数（help function）等

#### 2.2 Kconfig配置添加
1. 编辑`trunk/linux-4.4.x/net/netfilter/Kconfig`，添加模块配置项：

```
config NETFILTER_XT_MATCH_YOURMODULE
    tristate "Your Module match support"
    depends on NETFILTER_XTABLES  # 基本依赖
    # 避免使用select NETFILTER_XTABLES，防止递归依赖
    help
      This option adds a 'yourmodule' match for specific filtering needs.
      Description of what your module does.
```

#### 2.3 Makefile配置添加
1. 编辑`trunk/linux-4.4.x/net/netfilter/Makefile`，添加编译规则：

```
obj-$(CONFIG_NETFILTER_XT_MATCH_YOURMODULE) += xt_yourmodule.o
```

### 3. 模块编译与验证

#### 3.1 配置内核选项
1. 在设备特定配置文件中启用新模块：
   - 编辑`trunk/configs/boards/[设备名]/kernel-4.4.x.config`
   - 添加：`CONFIG_NETFILTER_XT_MATCH_YOURMODULE=y`

2. 在固件功能配置中启用：
   - 编辑`trunk/configs/templates/[设备名].config`
   - 添加：`CONFIG_FIRMWARE_INCLUDE_YOURMODULE=y`（如果需要功能开关）

#### 3.2 编译与测试流程
1. **清理环境**：
   ```bash
   make clean
   ```

2. **编译固件**：
   ```bash
   make [设备名] TOOLCHAIN=mipsel-linux-musl
   ```

3. **验证模块加载**：
   - 刷入新固件后，使用以下命令检查模块是否加载：
     ```bash
     lsmod | grep yourmodule
     ```
   - 检查iptables是否支持新匹配选项：
     ```bash
     iptables -m yourmodule --help
     ```

### 4. Web界面集成

#### 4.1 添加配置页面
1. 在Web界面源码中添加配置页面
2. 实现表单处理逻辑

#### 4.2 规则生成与应用
1. 编写脚本处理Web界面提交的配置
2. 生成并应用对应的iptables规则
3. 实现配置持久化保存

## 五、常见问题与解决方案

### 1. 模块编译失败
- **问题**：编译报错找不到头文件
  **解决**：检查包含路径，确保头文件位置正确

- **问题**：符号未定义错误
  **解决**：确保所有依赖模块都已启用，检查函数调用是否正确

### 2. 模块加载失败
- **问题**：内核版本不匹配
  **解决**：确保模块与内核版本一致

- **问题**：依赖项缺失
  **解决**：检查并启用所有必要的依赖模块

### 3. 规则不生效
- **问题**：iptables链顺序问题
  **解决**：调整规则在链中的位置，确保优先级正确

- **问题**：规则语法错误
  **解决**：检查规则语法，特别是模块参数格式

## 六、性能与优化建议

1. **规则数量控制**：大量SNI规则会影响性能，建议控制在合理范围内

2. **规则优化**：使用域名通配符减少规则数量，例如使用`*.example.com`代替多个子域名规则

3. **链结构设计**：合理设计iptables链结构，避免规则重复检查

4. **硬件加速**：对于高性能需求，考虑使用硬件NAT加速功能

---

通过本文档的指南，您可以成功在Padavan系统中增加SNI模块或其他自定义iptables扩展模块，实现更丰富的网络流量控制功能。关键是理解模块开发、内核配置、编译过程以及Web界面集成的完整流程，同时注意避免常见的配置问题，特别是递归依赖等陷阱。
