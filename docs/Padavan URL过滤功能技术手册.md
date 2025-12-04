# Padavan URL过滤功能技术手册

## 1. 技术概述

Padavan固件的URL过滤功能基于Netfilter框架实现，通过内核级别的网络流量分析与处理，提供对HTTP和HTTPS流量的内容过滤能力。本技术手册详细描述了该功能的实现原理、代码结构及技术特性。

## 2. 系统架构

URL过滤系统由三个核心组件组成，形成完整的过滤链路：

1. **规则管理层**：处理用户配置，生成过滤规则
2. **HTTP过滤引擎**：基于webstr模块，处理80端口流量
3. **HTTPS过滤引擎**：基于SNI模块，处理443端口流量

```
用户配置 → 规则管理层 → iptables规则链 → 匹配模块 → 流量控制
```

## 3. 核心模块实现

### 3.1 SNI过滤模块 (xt_sni_filter.c)

#### 3.1.1 技术原理

SNI (Server Name Indication) 过滤模块通过分析TLS握手过程中的ClientHello消息，提取其中的服务器名称字段，实现对HTTPS连接的域名过滤。

#### 3.1.2 关键数据结构

```c
// SNI信息结构体
typedef struct xt_sni_info {
    u_int8_t  invert;        // 反向匹配标志
    u_int8_t  len;           // SNI字符串长度
    u_int8_t  sni[SNI_MAX_LEN];  // SNI匹配字符串
} xt_sni_info_t;
```

#### 3.1.3 核心功能实现

1. **SNI提取函数**

```c
static int extract_sni_from_tls(const unsigned char *data, size_t data_len, 
                               unsigned char *output, size_t output_len) {
    // TLS记录层解析
    // 握手协议解析
    // 扩展字段遍历
    // SNI扩展提取
    // ...
}
```

2. **字符串匹配函数**

```c
static bool match_string_safe(const char *haystack, size_t haystack_len,
                             const char *needle, size_t needle_len) {
    const char *found;
    
    if (!haystack || !needle || needle_len == 0 || haystack_len < needle_len)
        return false;
        
    found = custom_memmem(haystack, haystack_len, needle, needle_len);
    return (found != NULL);
}
```

3. **主匹配函数**

```c
static bool xt_sni_match(const struct sk_buff *skb, struct xt_action_param *par) {
    // 协议类型检查
    // TCP头部获取
    // 有效载荷偏移计算
    // TLS记录类型验证
    // SNI提取与匹配
    // ...
}
```

### 3.2 webstr过滤模块 (xt_webstr.c)

#### 3.2.1 技术原理

webstr模块专门用于HTTP流量的内容过滤，通过解析HTTP请求头中的URL和Host字段，实现关键词匹配和过滤。

#### 3.2.2 关键数据结构

```c
// HTTP信息结构体
typedef struct httpinfo {
    char  host[HOSTSIZE];   // Host字段内容
    int hostlen;            // Host长度
    char  url[URLSIZE];     // URL路径
    int urllen;             // URL长度
} httpinfo_t;

// webstr匹配信息结构体
typedef struct xt_webstr_info {
    __u8 invert;            // 反向匹配标志
    __u8 type;              // 匹配类型（URL/HOST/CONTENT）
    __u16 len;              // 字符串长度
    char string[];          // 匹配字符串
} xt_webstr_info_t;
```

#### 3.2.3 核心功能实现

1. **HTTP信息提取**

```c
static int get_http_info(int flags, httpinfo_t *info, 
                        const unsigned char *data, unsigned int datalen) {
    // HTTP请求验证
    // Host字段提取
    // URL路径提取
    // ...
}
```

2. **线性搜索实现**

```c
static char *search_linear(char *needle, char *haystack, 
                          int needle_len, int haystack_len) {
    char *k = haystack + (haystack_len-needle_len);
    char *t = haystack;
    
    for(; t <= k; t++) {
        if (strncasecmp(t, needle, needle_len) == 0) return t;
    }
    
    return NULL;
}
```

3. **主匹配函数**

```c
static bool xt_webstr_match(const struct sk_buff *skb, struct xt_action_param *par) {
    // HTTP请求解析
    // 关键词列表处理
    // 匹配类型判断
    // 搜索与匹配
    // ...
}
```

### 3.3 规则生成与管理 (firewall_ex.c)

#### 3.3.1 技术原理

`include_webstr_filter`函数负责从用户配置中读取规则参数，生成相应的iptables规则，并根据协议类型分发到不同的过滤模块。

#### 3.3.2 核心功能实现

1. **MAC地址组处理**

```c
// MAC地址去重逻辑
char processed_macs[64][18];    // 存储唯一MAC地址
int unique_count = 0;           // 唯一MAC计数

// 使用strcmp()检测重复MAC
for (j = 0; j < unique_count; j++) {
    if (strcmp(processed_macs[j], mac_buf) == 0) {
        is_duplicate = 1;
        break;
    }
}
```

2. **规则生成逻辑**

```c
// 生成SNI过滤规则 - 针对HTTPS流量
for (int mac_idx = 0; mac_idx < mac_count; mac_idx++) {
    fprintf(fp, "-A %s -p tcp --dport 443 -m sni --sni \"%s\"%s -m mac --mac-source %s -j REJECT --reject-with tcp-reset\n",
        dtype, filterstr, url_timematch, mac_addresses[mac_idx]);
}

// 生成webstr过滤规则 - 针对HTTP流量
for (int mac_idx = 0; mac_idx < mac_count; mac_idx++) {
    fprintf(fp, "-A %s -p tcp --dport 80 -m webstr --url \"%s\"%s -m mac --mac-source %s -j REJECT --reject-with tcp-reset\n",
        dtype, url_list, url_timematch, mac_addresses[mac_idx]);
}
```

## 4. 技术特性与限制

### 4.1 字符串匹配机制

| 特性 | 描述 | 实现方式 | 限制 |
|------|------|---------|------|
| 字符串匹配 | 简单的子串包含匹配 | `custom_memmem` / `strncasecmp` | 不支持通配符 |
| 大小写敏感性 | HTTP匹配不区分大小写 | `strncasecmp` | HTTPS匹配区分大小写 |
| 关键词分隔 | 支持多关键词匹配 | `<&nbsp;>`分隔符 | 仅HTTP模块支持 |
| 路径处理 | 完整URL路径匹配 | 字符串直接比较 | 不支持路径模式匹配 |

### 4.2 技术限制

1. **通配符支持缺失**：
   - 当前实现不支持`*`和`?`等通配符
   - 所有匹配基于简单的子串包含关系

2. **HTTPS过滤限制**：
   - 仅能访问SNI字段，无法查看加密内容
   - 不支持没有SNI的HTTPS连接
   - 无法过滤加密后的URL路径

3. **性能考量**：
   - 线性搜索算法时间复杂度为O(n*m)
   - 大量规则会显著增加CPU负载
   - 规则数量建议控制在合理范围内

## 5. 技术实现深度分析

### 5.1 匹配算法分析

#### SNI匹配算法
- **实现方式**：基于`custom_memmem`的内存匹配
- **时间复杂度**：O(n*m)，其中n为SNI长度，m为关键词长度
- **特点**：简单直接，但效率受关键词数量影响

#### webstr匹配算法
- **实现方式**：`search_linear`线性搜索，使用`strncasecmp`进行大小写不敏感比较
- **时间复杂度**：O(n*m)，其中n为URL长度，m为关键词长度
- **优化**：使用关键词列表批量处理，但未使用更高效的算法如KMP

### 5.2 内存管理与安全

1. **缓冲区安全**：
   - 使用固定大小的栈缓冲区（`STACK_BUFFER_SIZE`）
   - 超出限制时切换到堆内存分配
   - 严格的长度检查和边界验证

2. **MAC地址处理**：
   - 最大支持64个唯一MAC地址
   - 实现了MAC地址格式验证
   - 自动去重避免规则冗余

3. **字符串操作安全**：
   - 所有字符串复制都有长度限制
   - 空指针和边界条件检查
   - 防止缓冲区溢出

## 6. 系统集成与调用链

### 6.1 规则加载流程

```
系统启动 → firewall_start → include_webstr_filter → 生成iptables规则 → 规则加载
```

### 6.2 数据包处理流程

1. **HTTP流量处理链**：
   ```
   数据包到达 → iptables FORWARD链 → webstr模块 → get_http_info → search_linear → 匹配结果
   ```

2. **HTTPS流量处理链**：
   ```
   数据包到达 → iptables FORWARD链 → SNI模块 → extract_sni_from_tls → match_string_safe → 匹配结果
   ```

## 7. 技术优化建议

### 7.1 算法优化

1. **高效字符串匹配算法**：
   - 实现KMP算法或Boyer-Moore算法，降低时间复杂度
   - 考虑使用Aho-Corasick算法同时匹配多个关键词

2. **通配符支持实现**：
   - 设计并实现支持`*`和`?`的通配符匹配算法
   - 可参考已有的`strglobmatch`函数进行集成

3. **正则表达式支持**：
   - 集成轻量级正则表达式引擎
   - 实现基本的正则模式匹配

### 7.2 架构优化

1. **规则缓存机制**：
   - 实现LRU缓存减少重复匹配
   - 预编译匹配规则提高处理速度

2. **规则优先级**：
   - 实现规则优先级排序
   - 高频匹配规则优先处理

3. **模块化重构**：
   - 提取公共匹配函数减少代码重复
   - 统一HTTP和HTTPS匹配接口

## 8. 技术诊断与调试

### 8.1 调试方法

1. **SNI模块调试**：
   ```bash
   # 启用SNI调试
   modprobe xt_sni enable_debug=1
   
   # 查看调试日志
   logread | grep -i "sni-filter"
   ```

2. **规则检查**：
   ```bash
   # 查看URL过滤相关规则
   iptables -L FORWARD -n -v | grep -E "webstr|sni"
   
   # 检查规则链
   iptables -L URL_LIST -n -v
   ```

3. **性能监控**：
   ```bash
   # 监控CPU使用情况
   top | grep -E "iptables|ksoftirqd"
   
   # 查看网络流量
   ifconfig br0
   ```

### 8.2 常见技术问题排查

1. **匹配失败问题**：
   - 检查关键词大小写（HTTPS区分大小写）
   - 验证SNI字段是否存在
   - 确认HTTP请求方法类型（仅支持GET/POST/HEAD）

2. **性能问题**：
   - 减少规则数量
   - 避免使用过于通用的关键词
   - 检查重复MAC地址是否已被正确去重

## 9. 技术参数与配置项

### 9.1 NVRAM配置参数

| 参数名 | 类型 | 说明 | 默认值 |
|-------|------|------|-------|
| url_enable_x | 布尔值 | 启用/禁用URL过滤 | 0 |
| url_num_x | 整数 | 过滤关键词数量 | 0 |
| url_keyword_xN | 字符串 | 第N个过滤关键词 | 空 |
| url_mac_x | 字符串 | 单个MAC地址过滤 | 空 |
| url_mac_group_x | 布尔值 | MAC组模式开关 | 0 |
| macfilter_num_x | 整数 | MAC地址数量 | 0 |
| macfilter_list_xN | 字符串 | 第N个MAC地址 | 空 |
| url_date_x | 字符串 | 星期设置（7位） | 空 |
| url_time_x | 字符串 | 时间段（HHMMHHMM） | 空 |

### 9.2 模块参数

| 模块 | 参数名 | 类型 | 说明 | 默认值 |
|------|-------|------|------|-------|
| xt_sni | enable_debug | 布尔值 | 启用调试日志 | 0 |
| xt_sni | save_failed_packets | 布尔值 | 保存解析失败的包 | 0 |
| xt_sni | max_saved_packets | 整数 | 最大保存包数量 | 100 |

## 10. 总结

Padavan固件的URL过滤功能通过结合SNI和webstr两个内核模块，实现了对HTTP和HTTPS流量的内容过滤能力。该实现采用了简单直接的字符串匹配策略，虽然在功能上存在一些限制（如不支持通配符），但具有实现简洁、资源占用低的特点。

通过技术优化，特别是算法改进和架构重构，可以进一步提升该功能的性能和灵活性，为用户提供更强大、更高效的URL过滤能力。

---

*本技术手册基于Padavan固件实际代码实现编写，确保技术描述的准确性和完整性。*
*最后更新时间：2025年12月3日*","}}}
