# AdByBy-Open

AdByBy-Open是一个开源的广告过滤代理程序，用于替换原有的adbyby二进制文件。
已集成原版adbyby的所有功能和接口。
K2P路由固件集成: https://github.com/tekintian/padavan

## 功能特性

- **HTTP代理服务**: 监听8118端口，提供HTTP代理功能
- **广告过滤**: 基于规则库过滤广告请求
- **规则管理**: 支持多种类型的过滤规则（简单匹配、域名匹配、通配符等）
- **实时统计**: 记录过滤统计信息
- **可配置性**: 支持命令行参数配置
- **轻量级**: C语言实现，内存占用小

## 编译安装

### 本地开发编译（用于测试）
```bash
cd trunk/user/adbyby/src
make clean && make
```

### 路由器交叉编译（生产环境）

**重要：** 为了在MIPS架构的路由器上运行，必须使用项目提供的交叉编译工具链进行编译。
工具链下载地址 https://github.com/tekintian/padavan/releases/tag/toolchain

```bash
cd trunk/user/adbyby/src

# 清理之前的编译文件
make clean

# 使用路由器工具链编译
CROSS_COMPILE=/Volumes/csdisk/padavan/toolchain/toolchain-mipsel-darwin_v4.4.x/bin/mipsel-linux-musl- make

# 验证编译结果
file adbyby
# 应该显示：ELF 32-bit LSB executable MIPS MIPS32 rel2 version 1
```

### 编译工具链说明

- **工具链路径**: `toolchain/toolchain-mipsel-darwin_v4.4.x/`
- **编译器版本**: GCC 13.2.0
- **目标架构**: MIPS32 (little-endian)
- **C库**: musl
- **交叉编译器**: `mipsel-linux-musl-gcc`

### 编译结果对比

| 特性 | 本地编译 | 交叉编译 |
|------|----------|----------|
| **架构** | x86_64 (Darwin) | MIPS32 (路由器) |
| **兼容性** | 仅开发测试 | 路由器运行 |
| **文件大小** | ~71KB | ~28KB |
| **链接库** | libSystem.B.dylib | ld-musl-mipsel.so.1 |

编译完成后会生成`adbyby`可执行文件，可以替换原有的`share/adbyby`文件用于路由器固件。

## 使用方法

### 基本使用
```bash
# 默认配置启动（监听8118端口）
./adbyby

# 指定端口
./adbyby -p 8080

# 调试模式
./adbyby -d

# 指定规则文件
./adbyby -r /path/to/rules.txt

# 前台运行（非守护进程）
./adbyby --no-daemon

# 显示统计信息
./adbyby -s

# 显示帮助
./adbyby -h
```

### 命令行参数
- `-p PORT`: 指定监听端口（默认8118）
- `-d`: 启用调试模式
- `-r FILE`: 指定规则文件路径
- `-s`: 显示统计信息并退出
- `-h`: 显示帮助信息
- `--no-daemon`: 在前台运行

## 规则格式

规则文件格式：`pattern|type|description`

### 规则类型
- `0`: 简单字符串匹配
- `1`: 正则表达式匹配
- `2`: 域名匹配
- `3`: URL匹配
- `4`: 通配符匹配

### 示例规则
```
# 简单字符串匹配
doubleclick|0|Google DoubleClick广告
advertisement|0|通用广告关键词

# 域名匹配
doubleclick.net|2|广告域名
googleadservices.com|2|Google广告服务

# 通配符匹配
*/ad/*|4|广告路径
*.doubleclick.*|4|DoubleClick通配符

# 带描述的规则
google-analytics.com|2|Google分析跟踪
facebook.com/tr|0|Facebook跟踪像素
```

## 内置规则

程序内置了常见的广告域名和URL模式：
- 主要广告服务提供商域名（Google、Facebook、Amazon等）
- 常见广告路径模式
- 跟踪和分析服务域名

## 配置文件位置

- 规则文件: `/usr/share/adbyby/data/rules.txt`
- PID文件: `/var/run/adbyby.pid`
- 日志输出: 标准输出/系统日志

## 与原版兼容性

本实现与原版adbyby的主要兼容性：

1. **端口兼容**: 默认监听8118端口
2. **配置兼容**: 读取相同的配置文件路径
3. **功能兼容**: 提供相同的广告过滤功能
4. **接口兼容**: 支持相同的启动参数

## 架构说明

```
adbyby.c          - 主程序入口
proxy.c/h         - HTTP代理处理
rules.c/h         - 规则管理引擎
utils.c/h         - 工具函数
```

### 主要模块

1. **主程序** (`adbyby.c`): 程序入口、命令行解析、主循环
2. **代理模块** (`proxy.c`): HTTP请求解析、响应生成
3. **规则模块** (`rules.c`): 规则加载、匹配、统计
4. **工具模块** (`utils.c`): 日志、URL解析、字符串处理

## 调试和监控

### 调试模式
```bash
./adbyby -d --no-daemon
```

调试模式会输出详细的请求和匹配信息。

### 统计信息
```bash
./adbyby -s
```

显示规则统计、命中次数等信息。

## 性能优化

- 使用内存池减少内存分配
- 优化的字符串匹配算法
- 异步I/O处理（可扩展）
- 规则缓存机制

## 扩展功能

本版本预留了扩展接口，可以轻松添加：
- HTTPS代理支持
- 更复杂的规则引擎
- Web管理界面
- 规则自动更新
- 统计数据持久化

## 注意事项

### 通用注意事项
1. 需要root权限运行（绑定端口<1024时）
2. 确保有足够的系统资源
3. 定期更新规则库以保持过滤效果
4. 在生产环境建议使用守护进程模式

### 路由器部署注意事项
1. **架构兼容性**: 必须使用MIPS交叉编译工具链编译
2. **编译环境**: 使用项目提供的`toolchain-mipsel-darwin_v4.4.x`工具链
3. **文件路径**: 确保可执行文件放置在`/usr/share/adbyby/`目录
4. **权限设置**: 确保adbyby文件具有执行权限（chmod +x）
5. **内存限制**: 路由器内存有限，注意监控内存使用情况
6. **固件集成**: 编译后需要正确打包到固件镜像中

## 故障排除

### 常见问题

1. **端口被占用**
   - 检查是否有其他程序占用8118端口
   - 使用`-p`参数指定其他端口

2. **规则文件不存在**
   - 程序会自动创建基本规则
   - 可以手动创建规则文件

3. **权限问题**
   - 确保有权限读取配置文件
   - 确保有权限创建PID文件

### 路由器环境问题

4. **架构不兼容**
   - 现象：程序无法启动，提示"Exec format error"
   - 解决：使用MIPS交叉编译工具链重新编译
   - 检查：使用`file adbyby`验证是否为ELF 32-bit LSB executable MIPS

5. **链接库缺失**
   - 现象：程序启动时报错找不到动态链接库
   - 解决：确保使用musl编译工具链
   - 检查：链接库应为`/lib/ld-musl-mipsel.so.1`

6. **内存不足**
   - 现象：程序运行时经常崩溃或被系统杀死
   - 解决：调整编译优化参数，减少内存使用
   - 检查：监控路由器内存使用情况

### 日志分析

调试模式下的日志格式：
```
[2024-01-10 15:30:45] INFO: Proxy server started on port 8118
[2024-01-10 15:30:46] DEBUG: Connection from 192.168.1.100:54321
[2024-01-10 15:30:46] DEBUG: Blocked by rule: doubleclick.net (hits: 1)
```

## 开发和贡献

欢迎提交Issue和Pull Request来改进这个项目。

### 编译要求

#### 本地开发编译
- GCC编译器
- 标准 POSIX 环境
- Make工具

#### 路由器交叉编译
- MIPS交叉编译工具链（项目提供）
- Make工具
- 兼容的构建环境

**推荐使用项目的交叉编译工具链：**
```bash
toolchain/toolchain-mipsel-darwin_v4.4.x/bin/mipsel-linux-musl-gcc
```

### 测试
```bash
make debug
./adbyby -d --no-daemon -p 8888
```

然后在浏览器中设置代理为`127.0.0.1:8888`进行测试。


## 与我联系
tekintian@gmail.com
QQ:932256355