# 增强版SNI域名/IP匹配函数（支持域名+IPv4+IPv6+通配符）
在SNI（Server Name Indication）中，**理论上允许传入IP地址（IPv4/IPv6）**，但实际场景中较少见（RFC 6066规范允许SNI字段为“FQDN或IP地址”，部分客户端会在无域名时填入IP）。因此路由器SNI过滤需要同时支持「域名通配符匹配」和「IP地址精准匹配」，下面是增强后的完整实现。

## 核心新增特性
1. **IP类型自动识别**：自动区分SNI中的IPv4、IPv6和域名；
2. **IP精准匹配**：IPv4（如 `192.168.1.1`）、IPv6（如 `2001:db8::1`）均支持大小写不敏感（IPv6字母可大写）、零压缩兼容（如 `2001:db8::1` 与 `2001:db8:0:0:0:0:0:1` 视为同一IP）；
3. **规则兼容**：过滤规则可是域名（含`*`/`?`）、IPv4、IPv6，函数自动适配匹配逻辑；
4. 保留原有特性：域名通配符（`*`/`?`）、大小写不敏感、嵌入式环境低资源占用。

## 完整实现（路由器专用）
```c
#include <stddef.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

// 域名/IP合法字符判断
#define IS_DOMAIN_CHAR(c) (isalnum((unsigned char)c) || (c) == '-' || (c) == '.')
#define IS_IPV4_CHAR(c) (isdigit((unsigned char)c) || (c) == '.')
#define IS_IPV6_CHAR(c) (isxdigit((unsigned char)c) || (c) == ':' || (c) == '%')  // %是IPv6范围标识符（可选支持）

// 辅助函数：判断字符串是否为IPv4地址（仅校验格式，不校验数值合法性）
static bool is_ipv4(const char *str) {
    if (str == NULL || *str == '\0') return false;
    int dot_cnt = 0;
    const char *p = str;
    while (*p != '\0') {
        if (!IS_IPV4_CHAR(*p)) return false;
        if (*p == '.') {
            dot_cnt++;
            // 连续点号（如192..1.1）或开头/结尾是点号（如.192.168.1）非法
            if (dot_cnt > 3 || (p == str) || (*(p+1) == '\0')) return false;
        }
        p++;
    }
    return (dot_cnt == 3);  // IPv4必须含3个点号（xxx.xxx.xxx.xxx）
}

// 辅助函数：判断字符串是否为IPv6地址（仅校验格式，不校验数值合法性）
static bool is_ipv6(const char *str) {
    if (str == NULL || *str == '\0') return false;
    int colon_cnt = 0;
    bool has_zero_compress = false;  // 是否含零压缩（::）
    const char *p = str;
    while (*p != '\0') {
        if (!IS_IPV6_CHAR(*p)) return false;
        if (*p == ':') {
            colon_cnt++;
            // 连续冒号（仅允许一次::）、开头/结尾冒号（除了::开头/结尾）
            if (colon_cnt > 7) return false;  // IPv6最多7个冒号（8段）
            if (p > str && *(p-1) == ':') {
                if (has_zero_compress) return false;  // 多次零压缩（如:::1）非法
                has_zero_compress = true;
            }
            // 结尾冒号且无零压缩（如2001:db8:::）非法
            if (*(p+1) == '\0' && !has_zero_compress) return false;
        }
        p++;
    }
    // 合法IPv6：冒号数≤7，且含零压缩时冒号数≤5（如::1含2个冒号，2001:db8::1含3个冒号）
    return (colon_cnt >= 1 && colon_cnt <= 7) && 
           (!has_zero_compress || colon_cnt <= 5);
}

// 辅助函数：IPv4地址精准匹配（大小写不敏感，格式容错）
static bool ipv4_match(const char *sni_ip, const char *rule_ip) {
    // 转换为小写（实际IPv4无字母，仅兼容异常场景）
    const char *s = sni_ip, *r = rule_ip;
    while (*s != '\0' && *r != '\0') {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*r)) return false;
        s++;
        r++;
    }
    // 需同时结束（避免规则是192.168.1，SNI是192.168.1.1的情况）
    return (*s == '\0' && *r == '\0');
}

// 辅助函数：IPv6地址零压缩展开（统一格式后匹配）
static void ipv6_expand(const char *src, char *dest, size_t dest_len) {
    memset(dest, 0, dest_len);
    char segments[8][5] = {0};  // IPv6共8段，每段最多4个十六进制字符
    int seg_idx = 0;
    const char *p = src;
    bool has_zero_compress = false;

    // 1. 解析原始IPv6为8段（处理零压缩::）
    while (*p != '\0' && seg_idx < 8) {
        if (*p == ':') {
            if (p > src && *(p-1) == ':') {
                has_zero_compress = true;
                p++;
                continue;
            }
            seg_idx++;
            p++;
        } else if (isxdigit((unsigned char)*p)) {
            // 复制段字符（转小写）
            int seg_len = strlen(segments[seg_idx]);
            if (seg_len < 4) {
                segments[seg_idx][seg_len] = tolower((unsigned char)*p);
            }
            p++;
        } else {
            p++;  // 跳过%等非核心字符（范围标识符）
        }
    }

    // 2. 填充零压缩的空段（如::1 → 前6段为0000）
    if (has_zero_compress) {
        int empty_segs = 8 - seg_idx - (seg_idx == 0 ? 0 : 1);  // 需补充的零段数
        for (int i = 7; i >= seg_idx + 1; i--) {
            strncpy(segments[i], segments[i - empty_segs], 4);
        }
        for (int i = seg_idx; i < seg_idx + empty_segs; i++) {
            strcpy(segments[i], "0000");
        }
    }

    // 3. 补全每段为4个字符（如1 → 0001，db8 → 0db8）
    for (int i = 0; i < 8; i++) {
        int len = strlen(segments[i]);
        if (len < 4) {
            char temp[5] = {0};
            memset(temp, '0', 4 - len);
            strcat(temp, segments[i]);
            strcpy(segments[i], temp);
        }
    }

    // 4. 拼接为标准格式（2001:0db8:0000:0000:0000:0000:0000:0001）
    for (int i = 0; i < 8; i++) {
        strcat(dest, segments[i]);
        if (i < 7) strcat(dest, ":");
    }
}

// 辅助函数：IPv6地址精准匹配（处理零压缩和大小写）
static bool ipv6_match(const char *sni_ip, const char *rule_ip) {
    char sni_expand[40] = {0};  // 展开后IPv6长度：8*4 +7=39（含冒号）
    char rule_expand[40] = {0};
    ipv6_expand(sni_ip, sni_expand, sizeof(sni_expand));
    ipv6_expand(rule_ip, rule_expand, sizeof(rule_expand));
    return (strcmp(sni_expand, rule_expand) == 0);
}

// 核心：域名通配符匹配（沿用之前优化后的逻辑）
static bool domain_wildcard_match(const char *sni, const char *rule) {
    const char *s = sni, *r = rule;
    const char *last_star = NULL, *s_backup = NULL;

    while (*s != '\0') {
        if (*r == '*') {
            last_star = r++;
            s_backup = s;
            while (*r == '*') r++;
            if (*r == '\0') return true;
        } else if ((*r == '?') || (tolower((unsigned char)*r) == tolower((unsigned char)*s) && IS_DOMAIN_CHAR(*s))) {
            s++;
            r++;
        } else if (last_star != NULL) {
            r = last_star + 1;
            s_backup++;
            s = s_backup;
            if (!IS_DOMAIN_CHAR(*s)) return false;
        } else {
            return false;
        }
    }

    while (*r == '*') r++;
    return (*r == '\0');
}

/**
 * @brief 增强版SNI匹配函数（支持域名通配符+IPv4+IPv6）
 * @param sni 待过滤的SNI字段（域名/IPv4/IPv6）
 * @param rule 过滤规则（域名含*?/IPv4/IPv6）
 * @return 匹配返回true，不匹配返回false
 */
static bool sni_full_match(const char *sni, const char *rule) {
    // 边界检查（路由器环境避免空指针）
    if (sni == NULL || rule == NULL || *sni == '\0' || *rule == '\0') {
        return false;
    }

    // 1. 识别SNI类型（IPv4/IPv6/域名）
    bool sni_is_ipv4 = is_ipv4(sni);
    bool sni_is_ipv6 = !sni_is_ipv4 && is_ipv6(sni);
    // 2. 识别规则类型（IPv4/IPv6/域名）
    bool rule_is_ipv4 = is_ipv4(rule);
    bool rule_is_ipv6 = !rule_is_ipv4 && is_ipv6(rule);

    // 3. 类型匹配逻辑（IP类型必须一致，否则直接不匹配）
    if (sni_is_ipv4 && rule_is_ipv4) {
        return ipv4_match(sni, rule);  // IPv4精准匹配
    } else if (sni_is_ipv6 && rule_is_ipv6) {
        return ipv6_match(sni, rule);  // IPv6精准匹配（兼容零压缩）
    } else if (!sni_is_ipv4 && !sni_is_ipv6 && !rule_is_ipv4 && !rule_is_ipv6) {
        return domain_wildcard_match(sni, rule);  // 域名通配符匹配
    } else {
        return false;  // 类型不匹配（如SNI是IP，规则是域名，直接不匹配）
    }
}
```

## 关键模块详解（适配路由器场景）
### 1. IP类型识别（轻量高效）
- `is_ipv4`：仅校验格式（3个点号、仅含数字和点），不校验数值（如 `256.0.0.1` 会被识别为IPv4格式，但实际匹配时因规则不相等会返回false，无需额外校验数值，减少CPU开销）；
- `is_ipv6`：校验冒号数量（1-7个）、零压缩合法性（仅允许一次 `::`），兼容IPv6的范围标识符（`%`），不校验段长度（后续展开时统一处理）。

### 2. IP匹配核心逻辑
- **IPv4匹配**：直接逐字符对比（IPv4无复杂格式，效率最高），支持大小写容错（异常场景兼容）；
- **IPv6匹配**：先将IPv6展开为标准格式（零压缩→完整8段，每段4个字符，如 `2001:db8::1` → `2001:0db8:0000:0000:0000:0000:0000:0001`），再逐字符对比，解决零压缩和大小写问题。

### 3. 资源占用优化（嵌入式重点）
- **无动态内存**：IPv6展开使用栈上数组（`segments[8][5]`、`sni_expand[40]`），总栈占用≤200字节，无内存泄漏风险；
- **低CPU开销**：IP识别和匹配均为线性扫描（O(n)时间），域名匹配最坏情况≤253次循环，对路由器CPU无压力；
- **格式校验简化**：仅校验“是否符合IP格式”，不校验“IP是否合法”（如 `256.0.0.1`），因过滤规则通常是合法IP，非法IP即使匹配也无实际意义，简化逻辑减少开销。

## 测试用例（覆盖所有核心场景）
```c
static void test_sni_full_match(void) {
    // 1. 域名通配符匹配（原有场景）
    const char *domain_cases[][2] = {
        {"abcqq.com", "*qq.com"},          // 匹配
        {"a.qq.com", "*.qq.com"},          // 匹配
        {"www.douyin.com", "www.?ouyin.com"},// 匹配
        {"qq.com.cn", "*qq.com"},          // 不匹配
    };

    // 2. IPv4匹配
    const char *ipv4_cases[][2] = {
        {"192.168.1.1", "192.168.1.1"},    // 匹配
        {"10.0.0.1", "10.0.0.2"},          // 不匹配
        {"192.168.0.1", "192.168.1.1"},    // 不匹配
    };

    // 3. IPv6匹配（兼容零压缩和大小写）
    const char *ipv6_cases[][2] = {
        {"2001:db8::1", "2001:DB8::1"},    // 匹配（大小写）
        {"2001:db8::1", "2001:db8:0:0:0:0:0:1"},// 匹配（零压缩）
        {"fe80::1", "fe80::2"},            // 不匹配
    };

    // 4. 类型不匹配（直接不匹配）
    const char *mismatch_cases[][2] = {
        {"192.168.1.1", "*qq.com"},        // IP vs 域名
        {"www.qq.com", "192.168.1.1"},     // 域名 vs IP
        {"2001:db8::1", "192.168.1.1"},    // IPv6 vs IPv4
    };

    // 执行测试（路由器环境可注释printf）
    #ifdef DEBUG
    for (size_t i = 0; i < sizeof(domain_cases)/sizeof(domain_cases[0]); i++) {
        printf("域名场景：SNI=%s 规则=%s → %s\n",
               domain_cases[i][0], domain_cases[i][1],
               sni_full_match(domain_cases[i][0], domain_cases[i][1]) ? "匹配" : "不匹配");
    }
    for (size_t i = 0; i < sizeof(ipv4_cases)/sizeof(ipv4_cases[0]); i++) {
        printf("IPv4场景：SNI=%s 规则=%s → %s\n",
               ipv4_cases[i][0], ipv4_cases[i][1],
               sni_full_match(ipv4_cases[i][0], ipv4_cases[i][1]) ? "匹配" : "不匹配");
    }
    for (size_t i = 0; i < sizeof(ipv6_cases)/sizeof(ipv6_cases[0]); i++) {
        printf("IPv6场景：SNI=%s 规则=%s → %s\n",
               ipv6_cases[i][0], ipv6_cases[i][1],
               sni_full_match(ipv6_cases[i][0], ipv6_cases[i][1]) ? "匹配" : "不匹配");
    }
    #endif
}
```

## 路由器集成建议
### 1. 规则管理优化
- 规则分类存储：将过滤规则按“域名”“IPv4”“IPv6”分类存储（如三个数组），匹配时先判断SNI类型，再仅遍历对应类型的规则，减少无效循环；
- 禁止IP规则含通配符：IP规则仅支持精准匹配（通配符对IP无实际意义），管理界面可限制IP规则不能输入`*`/`?`。

### 2. 性能优化（多规则场景）
- SNI预处理：一次识别SNI类型并缓存（避免多次调用`is_ipv4`/`is_ipv6`）；
- 规则排序：域名规则按“是否含*”排序（精确规则优先），IP规则无需排序（精准匹配效率已最高）。

### 3. 兼容性扩展（可选）
- 支持IPv6带端口格式：如 `[2001:db8::1]:80`，可在`is_ipv6`中补充处理（去掉`[]`和端口部分再匹配）；
- 支持IP段匹配：如 `192.168.1.0/24`，可新增`ipv4_cidr_match`函数（解析子网掩码后匹配），按需扩展。

## 总结
增强后的函数完全适配路由器SNI过滤的全场景需求：
- 支持域名通配符（`*`/`?`）、IPv4、IPv6三种类型的匹配；
- 低资源占用（栈上操作、线性时间、无动态内存），适合嵌入式环境；
- IP匹配兼容格式变体（IPv6零压缩、大小写），域名匹配符合原始需求；
- 代码模块化（IP识别、匹配、域名匹配分离），易于维护和扩展。

可直接集成到路由器固件中，无需额外依赖，兼容绝大多数SNI场景（包括域名、IP输入的情况）。