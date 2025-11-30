## 🚀 Padavan SNI过滤与MAC地址组模式优化

### 📋 功能概览

Padavan固件现已实现完整的SNI (Server Name Indication) 过滤模块，并针对MAC地址组模式进行了重大优化，提供更智能、更高效的HTTPS流量控制能力。

#### 🔧 核心组件

**1. SNI过滤内核模块**
- 文件：`trunk/linux-4.4.x/net/netfilter/xt_sni_filter.c`
- 功能：TLS ClientHello解析，支持TCP/UDP双协议
- 特性：IPv4/IPv6双栈支持，精确的SNI字段提取
- **新增**：强大的通配符域名匹配支持
- **新增**：智能路径规则分流机制

**2. 用户空间库**
- 文件：`trunk/user/iptables/iptables-1.8.7/extensions/libxt_sni.c`
- 功能：iptables命令行支持
- 参数：`--sni` 和 `--sni-domain` 域名匹配

**3. 防火墙核心优化**
- 文件：`trunk/user/rc/src/firewall_ex.c`
- **新增：MAC地址组模式智能去重**
- **优化：公共函数重构，消除代码重复**

### 🆕 通配符域名匹配支持

#### ✅ 多种通配符格式
- ***.domain.com 格式**：精确匹配主域名和所有子域名
  - 匹配：domain.com, sub.domain.com, sub2.sub1.domain.com
  - 不匹配：other-domain.com, domain-com.example.com

- ***domain.com 格式**：灵活匹配以特定后缀结尾的任何字符串
  - 匹配：domain.com, sub.domain.com, mydomain.com
  - 注意：可能包含误匹配，如not-a-domain.com也会被匹配

- **精确域名匹配**：高性能的完全匹配
  - 仅匹配：完全相同的域名

#### ✅ 智能路径规则分流
**自动检测并跳过路径规则**：
- 系统会自动识别包含路径模式（如 `/news`, `/ads`）的规则
- SNI模块将跳过包含路径的规则，由HTTP过滤模块处理
- 纯域名规则由SNI模块高效处理

```c
// 路径规则检测逻辑
for (i = 0; i < needle_len; i++) {
    if (needle[i] == '/') {
        DEBUGP("Rule contains path pattern, SNI module skipped: %s\n", needle);
        return false;  // 包含路径模式，SNI模块不处理
    }
}
```

### 🆕 MAC地址组模式优化特性

#### ✅ 智能去重机制
```c
// 新增MAC地址去重逻辑
char processed_macs[64][18];    // 存储唯一MAC地址
int unique_count = 0;           // 唯一MAC计数
int duplicate_count = 0;        // 重复MAC统计

// 使用strcmp()检测重复MAC
for (j = 0; j < unique_count; j++) {
    if (strcmp(processed_macs[j], mac_buf) == 0) {
        is_duplicate = 1;
        duplicate_count++;
        break;
    }
}
```

#### ✅ 性能优化
- **减少iptables规则数量**：自动去除重复MAC地址
- **内存安全**：限制最多64个唯一MAC地址
- **格式验证**：17字符长度验证，确保MAC地址格式正确
- **兼容保持**：单MAC模式功能完全保留

#### ✅ 代码重构
```c
// 公共函数：apply_url_mac_group_filter()
// 统一处理IPv4和IPv6过滤，遵循DRY原则
static void apply_url_mac_group_filter(FILE *fp, const char *dtype, 
                                     const char *lan_if, const char *timematch, 
                                     const char *IPT_CHAIN_NAME_URL_LIST)
```

### 🚀 使用方法

#### 编译固件
```bash
cd trunk
./configure --with-board=K2P
make
```

#### 基础SNI过滤
```bash
# 阻止特定域名（精确匹配）
iptables -A FORWARD -p tcp --dport 443 -m sni --sni "blocked-domain.com" -j DROP

# 阻止主域名及其所有子域名（*.domain.com格式）
iptables -A FORWARD -p tcp --dport 443 -m sni --sni "*.example.com" -j DROP

# 阻止所有以特定后缀结尾的域名（*domain.com格式）
iptables -A FORWARD -p tcp --dport 443 -m sni --sni "*qq.com" -j DROP

# 多个域名过滤
iptables -A FORWARD -p tcp --dport 443 -m sni --sni "facebook.com" -j DROP
iptables -A FORWARD -p tcp --dport 443 -m sni --sni "youtube.com" -j DROP

# 白名单模式
iptables -A FORWARD -p tcp --dport 443 -m sni ! --sni "allowed-domain.com" -j DROP
```

#### 智能规则分流示例
```bash
# 以下规则中包含路径，会被SNI模块自动跳过，由HTTP过滤模块处理
iptables -A FORWARD -p tcp --dport 443 -m sni --sni "qq.com/news" -j DROP  # SNI模块跳过

# 以下纯域名规则由SNI模块处理
iptables -A FORWARD -p tcp --dport 443 -m sni --sni "qq.com" -j DROP  # SNI模块处理
iptables -A FORWARD -p tcp --dport 443 -m sni --sni "*.qq.com" -j DROP  # SNI模块处理
iptables -A FORWARD -p tcp --dport 443 -m sni --sni "*qq.com" -j DROP  # SNI模块处理
```

#### MAC地址组模式配置
```bash
# 在Web界面配置：
# 1. 启用URL过滤
# 2. 设置macfilter_num_x（MAC地址数量）
# 3. 配置macfilter_list_x（MAC地址列表）
# 4. 启用url_mac_group_x（MAC地址组模式）

# 系统会自动：
# - 去除重复的MAC地址
# - 为每个唯一MAC创建iptables规则
# - 支持时间匹配和反向匹配
```

### 📊 性能对比

| 特性 | 优化前 | 优化后 |
|------|--------|--------|
| 重复MAC处理 | 创建重复规则 | 智能去重 |
| 内存使用 | 无限制 | 安全限制64个 |
| 代码维护 | 重复代码 | 公共函数 |
| 性能影响 | 冗余规则 | 精简规则 |
| IPv4/IPv6 | 分离实现 | 统一处理 |
| 域名匹配 | 仅精确匹配 | 支持通配符匹配 |
| 规则分流 | 无分流机制 | 智能路径规则分流 |

### 🔍 技术细节

#### 域名匹配算法
1. **精确匹配**：最快速的完全字符串比较
2. ***.domain.com 格式**：
   - 检查是否为主域名（domain.com）
   - 检查是否为子域名（sub.domain.com）
3. ***domain.com 格式**：
   - 检查字符串末尾是否匹配指定后缀
4. **路径规则检测**：
   - 快速扫描规则中是否包含'/'字符
   - 包含则自动跳过，避免无效处理

#### MAC地址去重算法
1. **输入处理**：遍历macfilter_list_x中的所有MAC地址
2. **格式验证**：检查17字符长度（XX:XX:XX:XX:XX:XX）
3. **重复检测**：使用strcmp()比较已处理的MAC地址
4. **规则生成**：只为唯一MAC地址创建iptables规则
5. **统计优化**：记录去除的重复数量（调试用）

#### 时间匹配支持
```c
// 支持时间范围匹配
char mac_timematch[256] = {0};
strcpy(mac_timematch, timematch);
strcat(mac_timematch, " -m mac");
if (nvram_match("url_inv_x", "1"))
    strcat(mac_timematch, " !");    // 反向匹配支持
```

#### 反向匹配功能
```bash
# 反向匹配：除指定MAC外的所有设备
iptables -A FORWARD -i br0 -m mac ! --mac-source XX:XX:XX:XX:XX:XX -j URL_FILTER
```

### 🎯 应用场景

#### 1. 家长控制
- **SNI过滤**：阻止儿童访问社交媒体、游戏网站
- **MAC组模式**：为孩子的所有设备统一设置过滤规则
- **时间控制**：设置学习/睡觉时间段限制
- **通配符支持**：使用*.domain.com一次性阻止整个域名家族

#### 2. 企业网络管理
- **SNI过滤**：限制员工访问娱乐、购物网站
- **MAC组模式**：按部门或员工组设置不同过滤策略
- **性能优化**：减少重复规则，提升网关性能
- **智能分流**：系统自动处理路径级和域名级规则

#### 3. 网络安全
- **SNI过滤**：阻止访问恶意域名、钓鱼网站
- **MAC组模式**：为高风险设备设置严格过滤
- **智能去重**：避免规则冗余，提高处理效率
- **通配符保护**：使用*domain.com格式阻止所有变体域名

#### 4. 公共场所WiFi
- **SNI过滤**：合规性内容过滤
- **MAC组模式**：为不同用户群体设置差异化策略
- **IPv6支持**：完整支持现代双栈网络
- **智能分流**：无需手动区分路径和域名规则

### 📈 优化效果

- ✅ **规则精简**：自动去除重复MAC地址，减少iptables规则数量
- ✅ **性能提升**：更少的规则意味着更快的包处理速度
- ✅ **内存安全**：限制MAC地址数量，防止内存溢出
- ✅ **代码质量**：DRY原则，消除重复代码，提高可维护性
- ✅ **功能完整**：保持原有单MAC模式，增加智能组模式
- ✅ **兼容性强**：支持IPv4/IPv6双栈，时间匹配，反向匹配
- ✅ **通配符支持**：灵活的域名匹配能力
- ✅ **智能分流**：自动区分处理路径和域名规则

### 🔧 调试与监控

```bash
# 查看当前过滤规则
iptables -L FORWARD -n | grep -E "(sni|mac)"

# 统计过滤命中次数
iptables -L FORWARD -n -v | grep -E "(sni|mac)"

# 查看系统日志
logread | grep -i "url.*filter"

# 启用SNI模块调试
modprobe xt_sni enable_debug=1
logread | grep -i "sni-filter"

# 检查重复MAC去除情况（需启用调试）
# 在firewall_ex.c中取消注释调试输出
```

这个优化后的SNI过滤系统为Padavan固件提供了企业级的HTTPS流量控制能力，结合智能MAC地址组管理、灵活的通配符支持和智能规则分流，实现了高效、灵活、安全的网络访问控制解决方案。