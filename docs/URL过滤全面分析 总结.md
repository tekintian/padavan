# URL过滤全面分析 总结


## Padavan固件URL过滤系统完整分析

### 1. 核心组件架构

#### Netfilter内核模块：
- **xt_webstr模块**：处理HTTP流量(URL字符串匹配)
- **xt_sni_filter模块**：处理HTTPS流量(SNI域名提取和匹配)

#### iptables用户空间扩展：
- **libxt_webstr.c**：为xt_webstr提供命令行接口
- **libxt_sni.c**：为xt_sni_filter提供命令行接口

### 2. 技术实现细节

#### xt_sni_filter模块特点：
- 专门处理HTTPS的TLS ClientHello消息中的SNI字段
- 支持三种匹配模式：
  1. 精确匹配（domain.com）
  2. 子域名匹配（*.domain.com）
  3. 后缀匹配（*domain.com）
- 自动跳过包含路径的规则（含有"/"的模式）
- 包含详细的调试日志输出功能

#### xt_webstr模块特点：
- 处理HTTP流量的头部字符串匹配
- 支持HOST、URL、CONTENT三种过滤类型
- 使用Boyer-Moore算法进行高效字符串匹配
- 定义最大长度限制：BM_MAX_NLEN=256，BM_MAX_HLEN=1024

### 3. 防火墙集成机制

#### 模块加载机制：
```c
// 当URL过滤启用时动态加载内核模块
if (nvram_match("url_enable_x", "1")) {
    if (!module_smart_load("xt_webstr", NULL))
        logmessage("Firewall", "ERROR: Failed to load xt_webstr module");
    
    if (!module_smart_load("xt_sni_filter", NULL))
        logmessage("Firewall", "ERROR: Failed to load xt_sni_filter module");
}
```

#### iptables规则生成：
系统会为每个URL过滤条目生成两种规则：

1. **HTTPS SNI规则**（端口443）：
   ```bash
   -A urllist -p tcp --dport 443 -m sni --sni "blocked-domain.com" -j REJECT --reject-with tcp-reset
   ```

2. **HTTP Webstr规则**（端口80）：
   ```bash
   -A urllist -p tcp --dport 80 -m webstr --url "blocked-domain.com" -j REJECT --reject-with tcp-reset
   ```

### 4. 配置和处理流程

#### 关键NVRAM参数：
- `url_enable_x`：全局启用/禁用URL过滤
- `url_num_x`：URL过滤条目数量
- `url_keyword_xN`：具体的URL过滤关键词
- `url_mac_group_x`：启用MAC组过滤模式
- `url_mac_x`：单个MAC地址过滤
- `url_inv_x`：反转匹配逻辑

#### 处理逻辑：
1. 系统启动时根据配置决定是否加载模块
2. 对每个URL关键词进行处理和清洗（去除http://或https://前缀）
3. 分别为HTTPS和HTTP生成对应的SNI和webstr规则
4. 支持MAC地址绑定，可限定特定设备的URL过滤
5. 规则优化：尽可能合并多个URL到单条规则中

#### MAC地址处理优化：
- 支持单一MAC和MAC组两种模式
- 自动去重避免生成冗余规则
- 提高多设备环境下规则执行效率

### 5. 性能和安全性考虑

#### 性能优化措施：
- 字符串匹配采用Boyer-Moore算法提升效率
- 规则合并减少iptables规则数量
- MAC地址去重避免重复规则
- 连接跟踪机制绕过已建立连接

#### 安全特性：
- 输入长度验证防止缓冲区溢出
- TLS记录结构验证确保SNI提取安全
- 完整的错误处理和日志记录机制

这套实现提供了对HTTP和HTTPS流量的全面URL过滤能力，具有灵活的配置选项和良好的性能表现。
