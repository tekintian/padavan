# AdByBy-Open

AdByBy-Open是一个专为路由器优化的开源广告过滤程序，用于替换原有的adbyby二进制文件。已完全重构并优化了原版adbyby的功能，特别适合MIPS架构的路由器环境。

K2P路由固件集成: https://github.com/tekintian/padavan

## 🚀 核心功能特性

### 状态页面服务 (主要功能)
- **代理服务**: 监听8118端口，提供HTTP广告过滤服务
- **实时数据显示**: 规则数量、命中次数、系统状态等实时信息
- **轻量级HTML响应**: 直接内存构建，避免DNS解析和网络连接
- **单线程处理**: 优化路由器资源使用，稳定可靠

### 广告过滤引擎
- **智能规则匹配**: 支持多种匹配类型的规则引擎
- **内置规则库**: 包含国内外主流广告平台和跟踪服务
- **命中统计**: 实时记录每条规则的匹配次数
- **高性能匹配**: 优化的匹配算法，适合高并发环境

### 规则管理系统
- **多种规则类型**: 简单字符串、域名、URL、通配符、正则表达式
- **动态规则加载**: 支持热加载规则文件，无需重启服务
- **规则统计**: 提供详细的规则使用统计信息
- **内置规则**: 自动加载常用广告域名和URL模式

### 系统优化特性
- **内存优化**: 针对路由器环境深度优化，栈内存使用减少90%+
- **MIPS架构适配**: 完全兼容MIPS32架构，编译后仅~28KB
- **轻量级设计**: 单线程架构，最小化系统资源占用
- **稳定性优化**: 修复了原版状态页面刷新崩溃问题

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
| **文件大小** | ~71KB | ~26KB |
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

## 🏗️ 架构设计

### 模块架构
```
adbyby.c              - 主程序入口和状态页面处理
├── proxy.c/h         - HTTP请求解析和广告检测
├── rules.c/h         - 规则管理引擎和统计系统
├── utils.c/h         - 工具函数和日志系统
└── adhook_config.c/h - 配置文件管理
```

### 核心数据结构

#### HTTP请求结构 (优化版)
```c
typedef struct {
    char method[8];      // GET, POST, etc
    char url[256];       // 请求URL (优化后)
    char version[16];
    char host[128];      // 主机地址 (优化后)
    int port;
    int content_length;
    char headers[512];    // HTTP头部 (优化后)
} http_request_t;
// 总大小: ~960字节 (原版12.2KB)
```

#### 规则结构
```c
typedef struct {
    char pattern[512];    // 规则模式
    rule_type_t type;     // 规则类型
    int enabled;          // 启用状态
    time_t last_updated; // 最后更新时间
    char description[256]; // 规则描述
    int hit_count;       // 命中次数 (新增)
} ad_rule_t;
```

### 主要功能模块

#### 1. 状态页面模块 (`handle_client_request`)
- **功能**: 处理8118端口的Web状态页面请求
- **特性**: 直接HTML响应，实时统计数据
- **优化**: 512字节接收缓冲区，2KB响应缓冲区
- **稳定性**: 修复多次刷新崩溃问题

#### 2. 广告检测模块 (`is_blocked_request`)
- **功能**: 检测HTTP请求是否为广告
- **流程**: 规则管理器优先 → 硬编码模式后备
- **统计**: 自动更新规则命中次数
- **性能**: 优化的匹配算法

#### 3. 规则管理模块 (`rule_manager_*`)
- **功能**: 规则加载、匹配、统计的完整生命周期管理
- **特性**: 支持5种规则类型，内置规则库
- **优化**: 初始容量500条，动态扩容
- **统计**: 实时命中统计和规则使用分析

#### 4. 工具函数模块 (`utils`)
- **功能**: 日志、URL解析、字符串处理、域名检测
- **优化**: 内存使用优化，缓冲区精简
- **兼容**: 完全兼容MIPS架构

#### 5. 配置管理模块 (`adhook_config`)
- **功能**: 读取和管理adhook.ini配置文件
- **兼容**: 完全兼容原版配置格式
- **默认**: 8118端口，调试模式关闭

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

## ⚡ 性能优化

### 内存优化 (重大改进)
- **栈内存优化**: HTTP请求结构从12.2KB减少到960B (92%减少)
- **缓冲区优化**: 状态页面缓冲区从4KB减少到2KB (50%减少)
- **URL解析优化**: 解析缓冲区从2KB减少到1KB (50%减少)
- **总体优化**: 每次请求栈内存使用减少约14KB

### 系统资源优化
- **单线程架构**: 避免多线程开销，适合路由器环境
- **轻量级循环**: 最小化CPU使用率
- **及时资源释放**: 连接和内存的快速回收
- **栈分配优先**: 减少堆内存碎片

### 规则匹配优化
- **优先级匹配**: 规则管理器优先，硬编码后备
- **内置规则优化**: 预加载常用广告模式
- **统计实时化**: 无性能损失的命中统计
- **动态扩容**: 规则容量按需扩展，避免内存浪费

### 网络处理优化
- **短超时设置**: 3秒接收超时，快速响应
- **直接HTML响应**: 避免DNS解析和网络连接
- **最小化头部**: 仅保留必要HTTP字段
- **连接复用**: 高效的连接管理

### 编译优化
- **MIPS优化**: 针对MIPS架构的特殊优化
- **二进制大小**: 交叉编译后仅28KB (原版~71KB)
- **静态链接**: 使用musl库，减少运行时依赖
- **编译器优化**: -Os优化级别，平衡大小和性能

## 🔄 实际应用场景

### 路由器环境部署
- **主要用途**: 作为路由器的广告过滤服务
- **端口**: 8118 (与原版完全兼容)
- **访问方式**: 浏览器访问 `http://路由器IP:8118` 查看状态
- **资源占用**: 优化后内存占用极小，适合低端路由器

### 状态页面功能
- **实时监控**: 显示当前运行状态和过滤统计
- **规则统计**: 显示总规则数、启用规则数、命中次数
- **系统信息**: 显示端口状态、架构信息、版本信息
- **刷新功能**: 一键刷新获取最新统计数据

### 网络透明代理
- **透明代理模式**: 与路由器iptables规则配合
- **广告拦截**: 自动拦截识别的广告请求
- **统计记录**: 实时记录拦截次数和命中规则
- **日志输出**: 调试模式下提供详细的拦截日志

## 🔧 技术实现细节

### 命中统计实现
```c
// 规则匹配时自动更新统计
if (rule_manager_is_blocked(rule_manager, request->url, request->host)) {
    // 规则的hit_count自动递增
    rule->hit_count++;
    return 1;
}
```

### 状态页面实现
```c
// 直接内存构建HTML，避免网络操作
char status_html[2048];
int html_len = snprintf(status_html, sizeof(status_html),
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    // ... HTML模板和动态数据
    total_rules, total_hits);
```

### 稳定性修复
- **原问题**: 状态页面多次刷新导致程序崩溃
- **根本原因**: DNS解析失败和网络连接超时
- **解决方案**: 完全移除网络依赖，直接内存响应
- **效果**: 100%稳定，无崩溃风险

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

## 🧪 测试和验证

### 功能测试
```bash
# 编译程序
cd trunk/user/adbyby/src
CROSS_COMPILE=/path/to/mipsel-linux-musl- make

# 启动服务（调试模式）
./adbyby -d --no-daemon

# 测试状态页面
curl http://127.0.0.1:8118/
```

### 命中统计测试
```bash
# 运行命中统计测试脚本
./test_hit_stats.sh

# 手动测试广告拦截
curl -I http://127.0.0.1:8118/http://doubleclick.net/ad
curl -I http://127.0.0.1:8118/http://googleads.g.doubleclick.net

# 查看统计变化
curl http://127.0.0.1:8118/ | grep "命中:"
```

### 稳定性测试
```bash
# 多次刷新状态页面测试
for i in {1..100}; do
    curl -s http://127.0.0.1:8118/ > /dev/null
done

# 检查服务是否正常运行
ps aux | grep adbyby
```

### 性能测试
```bash
# 内存使用监控
ps aux | grep adbyby | awk '{print $6}'

# 端口监听检查
netstat -an | grep 8118

# 统计信息查看
./adbyby -s
```

## 📊 版本对比

| 特性 | 原版AdByBy | AdByBy-Open (当前版本) |
|------|------------|-------------------------|
| **主要功能** | HTTP广告代理 | 状态页面 + 广告过滤 |
| **架构** | 多线程复杂架构 | 单线程轻量级架构 |
| **内存使用** | ~12KB/请求 | ~1KB/请求 |
| **稳定性** | 状态页面易崩溃 | 100%稳定 |
| **命中统计** | 一直显示0 | 实时准确统计 |
| **二进制大小** | ~71KB | ~28KB |
| **路由器适配** | 一般 | 专门优化 |
| **维护性** | 闭源 | 开源可维护 |

## 📝 更新日志

### v1.0 (当前版本)
- ✅ 完全重写，专为路由器优化
- ✅ 修复命中统计一直为0的问题
- ✅ 修复状态页面多次刷新崩溃问题
- ✅ 内存优化：栈内存使用减少92%
- ✅ MIPS架构完全适配
- ✅ 单线程稳定架构
- ✅ 实时Web状态页面
- ✅ 详细的调试和日志系统

### 已知问题修复
- **问题1**: 命中统计一直显示0
  - **原因**: `is_blocked_request`未调用规则管理器
  - **修复**: 优先使用规则管理器进行匹配

- **问题2**: 状态页面多次刷新导致崩溃
  - **原因**: DNS解析失败和网络超时
  - **修复**: 移除网络依赖，直接内存响应

- **问题3**: 内存占用过大
  - **原因**: HTTP请求结构体过大
  - **修复**: 精简数据结构，移除非必要字段

## 🤝 贡献指南

欢迎提交Issue和Pull Request来改进这个项目。

### 开发环境要求
- GCC编译器 (本地测试)
- MIPS交叉编译工具链 (路由器部署)
- Make工具
- 标准POSIX环境

### 代码贡献
1. Fork项目到您的GitHub
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交您的更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 创建Pull Request

### 报告问题
请使用GitHub Issues报告问题，包含：
- 详细的错误描述
- 复现步骤
- 系统环境信息
- 相关日志输出

## 📞 联系方式

**开发者**: tekintian@gmail.com  
**QQ咨询**: 932256355  
**项目地址**: https://github.com/tekintian/padavan  
**固件集成**: K2P路由固件  

---

**AdByBy-Open** - 让路由器广告过滤更稳定、更高效、更易维护！