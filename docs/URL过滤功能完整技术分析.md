## URL过滤功能完整技术分析

### 1. 总体架构概述

URL过滤功能采用多层架构实现：
- **Web管理界面**：`Advanced_URLFilter_Content.asp` - 用户配置界面
- **用户空间**：`firewall_ex.c` - 防火墙规则生成和管理
- **iptables扩展**：`libxt_sni.c` - SNI匹配模块的用户空间接口
- **内核空间**：`xt_sni.c` - 内核级别的SNI包匹配
- **头文件**：`xt_sni.h` - 统一的数据结构定义

### 2. Web管理界面详细分析

#### 2.1 界面功能组件
```javascript
// URL过滤开关控制
function change_url_enable(){
    var v = document.form.url_enable_x[0].checked;
    showhide_div('tbl_urlf_main', v);
}

// MAC地址模式切换
function changeMacMode() {
    var mode = document.form.url_mac_group_x.value;
    if (mode == "1") {
        $('single_mac_div').style.display = 'none';    // MAC组模式
        $('mac_group_div').style.display = 'block';
    } else {
        $('single_mac_div').style.display = 'block';    // 单MAC模式
        $('mac_group_div').style.display = 'none';
    }
}
```

#### 2.2 配置参数
- **url_enable_x**: URL过滤总开关
- **url_date_x**: 生效日期（0-6对应周日到周六）
- **url_time_x**: 生效时间段
- **url_mac_group_x**: MAC地址模式选择（0=单MAC，1=MAC组）
- **url_mac_x**: 单个MAC地址
- **url_inv_x**: 反向匹配标志
- **url_keyword_x**: URL关键词列表

### 3. 用户空间防火墙规则生成

#### 3.1 核心函数：`include_webstr_filter()`

```c:2000-2100
static int include_webstr_filter(FILE *fp)
{
    int webstr_items, url_length, url_total;
    char url_list[256], nv_name[32], url_buf[256], *filterstr;
    char url_timematch[256];
    const char *dtype = IPT_CHAIN_NAME_URL_LIST;
    
    // 获取时间匹配条件
    timematch_conv(url_timematch, "url_date_x", "url_time_x");
    
    // MAC地址处理逻辑
    int mac_count = 0;
    char mac_addresses[64][18];
    int need_mac_condition = 0;
    
    if (nvram_match("url_mac_group_x", "1")) {
        // MAC组模式：从MAC过滤器获取地址列表
        mac_count = nvram_get_int("macfilter_num_x");
        // 收集唯一MAC地址，去重处理
        for (int mac_idx = 0; mac_idx < mac_count; mac_idx++) {
            mac_conv("macfilter_list_x", mac_idx, mac_buf);
            // 去重逻辑...
        }
    } else {
        // 单MAC模式
        mac_conv("url_mac_x", -1, mac_buf);
    }
}
```

#### 3.2 规则生成策略

```c:2100-2200
foreach_x("url_num_x") {
    sprintf(nv_name, "url_keyword_x%d", i);
    filterstr = nvram_safe_get(nv_name);
    
    // 清理URL前缀
    if (strncasecmp(url_buf, "http://", 7) == 0)
        filterstr = url_buf + 7;
    else if (strncasecmp(url_buf, "https://", 8) == 0)
        filterstr = url_buf + 8;
    
    // 生成HTTPS流量过滤规则（基于SNI模块）
    if (need_mac_condition) {
        for (int mac_idx = 0; mac_idx < mac_count; mac_idx++) {
            fprintf(fp, "-A %s -p tcp --dport 443 -m sni --sni \"%s\" --algo bm%s -m mac --mac-source %s -j REJECT --reject-with tcp-reset\n",
                dtype, filterstr, url_timematch, mac_addresses[mac_idx]);
        }
    } else {
        fprintf(fp, "-A %s -p tcp --dport 443 -m sni --sni \"%s\" --algo bm%s -j REJECT --reject-with tcp-reset\n",
            dtype, filterstr, url_timematch);
    }
    
    // 生成HTTP流量过滤规则
    // 类似的逻辑，端口80
}
```

#### 3.3 MAC地址组优化处理

```c:1500-1600
static void apply_url_mac_group_filter(FILE *fp, const char *dtype, const char *lan_if, const char *timematch, const char *chain_name)
{
    int mac_count = nvram_get_int("macfilter_num_x");
    if (mac_count > 0) {
        char processed_macs[64][18]; // 存储唯一MAC地址
        int unique_count = 0;
        int duplicate_count = 0;
        
        // 去重逻辑
        for (i = 0; i < mac_count; i++) {
            mac_conv("macfilter_list_x", i, mac_buf);
            if (strlen(mac_buf) == 17) {
                int is_duplicate = 0;
                for (j = 0; j < unique_count; j++) {
                    if (strcmp(processed_macs[j], mac_buf) == 0) {
                        is_duplicate = 1;
                        duplicate_count++;
                        break;
                    }
                }
                if (!is_duplicate && unique_count < 64) {
                    strcpy(processed_macs[unique_count], mac_buf);
                    unique_count++;
                    // 创建iptables规则
                    fprintf(fp, "-A %s -i %s%s -j %s\n", dtype, lan_if, mac_timematch, chain_name);
                }
            }
        }
    }
}
```

### 4. iptables扩展模块分析

#### 4.1 用户空间接口：`libxt_sni.c`

```c:50-100
// 命令行参数定义
static const struct xt_option_entry sni_opts[] = {
    {.name = "from", .id = O_FROM, .type = XTTYPE_UINT16,
     .flags = XTOPT_PUT, XTOPT_POINTER(s, from_offset)},
    {.name = "to", .id = O_TO, .type = XTTYPE_UINT16,
     .flags = XTOPT_PUT, XTOPT_POINTER(s, to_offset)},
    {.name = "algo", .id = O_ALGO, .type = XTTYPE_STRING,
     .flags = XTOPT_MAND | XTOPT_PUT, XTOPT_POINTER(s, algo)},
    {.name = "sni", .id = O_STRING, .type = XTTYPE_STRING,
     .flags = XTOPT_INVERT, .excl = F_HEX_STRING},
    {.name = "hex-sni", .id = O_HEX_STRING, .type = XTTYPE_STRING,
     .flags = XTOPT_INVERT, .excl = F_STRING},
    {.name = "icase", .id = O_ICASE, .type = XTTYPE_NONE},
    XTOPT_TABLEEND,
};
```

#### 4.2 字符串解析处理

```c:100-150
static void parse_string(const char *s, struct xt_sni_info *info)
{
    if (strlen(s) <= XT_SNI_MAX_PATTERN_SIZE) {
        strncpy(info->pattern, s, XT_SNI_MAX_PATTERN_SIZE);
        info->patlen = strnlen(s, XT_SNI_MAX_PATTERN_SIZE);
        return;
    }
    xtables_error(PARAMETER_PROBLEM, "SNI too long \"%s\"", s);
}

static void parse_hex_string(const char *s, struct xt_sni_info *info)
{
    // 处理十六进制字符串
    // 支持| |包围的十六进制格式
    // 处理转义字符和字面量
}
```

### 5. 内核空间模块分析

#### 5.1 核心匹配函数：`sni_mt()`

```c:30-50
static bool sni_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
    const struct xt_sni_info *conf = par->matchinfo;
    bool invert;

    invert = conf->u.v1.flags & XT_SNI_FLAG_INVERT;

    // 使用内核textsearch框架进行模式匹配
    return (skb_find_text((struct sk_buff *)skb, conf->from_offset,
                         conf->to_offset, conf->config)
                         != UINT_MAX) ^ invert;
}
```

#### 5.2 模块初始化和验证

```c:50-80
static int sni_mt_check(const struct xt_mtchk_param *par)
{
    struct xt_sni_info *conf = par->matchinfo;
    struct ts_config *ts_conf;
    int flags = TS_AUTOLOAD;

    // 参数验证
    if (conf->from_offset > conf->to_offset)
        return -EINVAL;
    if (conf->patlen > XT_SNI_MAX_PATTERN_SIZE)
        return -EINVAL;
    
    // 大小写不敏感标志
    if (conf->u.v1.flags & XT_SNI_FLAG_IGNORECASE)
        flags |= TS_IGNORECASE;
    
    // 准备文本搜索配置
    ts_conf = textsearch_prepare(conf->algo, conf->pattern, conf->patlen,
                                 GFP_KERNEL, flags);
    if (IS_ERR(ts_conf))
        return PTR_ERR(ts_conf);

    conf->config = ts_conf;
    return 0;
}
```

### 6. 数据结构定义

#### 6.1 核心数据结构：`xt_sni.h`

```c:14-32
struct xt_sni_info {
    __u16 from_offset;                    // 搜索起始偏移
    __u16 to_offset;                      // 搜索结束偏移
    char   algo[XT_SNI_MAX_ALGO_NAME_SIZE]; // 匹配算法名称
    char   pattern[XT_SNI_MAX_PATTERN_SIZE]; // 模式字符串
    __u8  patlen;                         // 模式长度
    union {
        struct {
            __u8  invert;               // 反转标志（v0版本）
        } v0;
        struct {
            __u8  flags;                // 标志位（v1版本）
        } v1;
    } u;
    // 内核使用的textsearch配置
    struct ts_config __attribute__((aligned(8))) *config;
};
```

#### 6.2 标志位定义

```c:9-12
enum {
    XT_SNI_FLAG_INVERT     = 0x01,  // 反向匹配
    XT_SNI_FLAG_IGNORECASE = 0x02,  // 忽略大小写
};
```

### 7. 技术特色和优化

#### 7.1 性能优化
- **去重处理**：MAC地址自动去重，避免重复规则
- **textsearch框架**：使用内核高效的文本搜索算法
- **规则合并**：HTTP规则支持批量处理，减少规则数量

#### 7.2 功能特色
- **SNI匹配**：针对HTTPS流量的SNI字段匹配
- **时间控制**：支持按时间段和日期过滤
- **MAC绑定**：支持单个MAC或MAC组模式
- **算法选择**：支持多种字符串匹配算法（bm, kmp等）

#### 7.3 安全考虑
- **长度限制**：模式字符串最大128字节
- **内存对齐**：内核配置结构8字节对齐
- **错误处理**：完整的参数验证和错误处理

### 8. 工作流程

1. **配置阶段**：用户通过Web界面配置过滤规则
2. **规则生成**：`firewall_ex.c`读取配置，生成iptables规则
3. **规则加载**：iptables加载SNI匹配模块
4. **包处理**：内核模块对数据包进行SNI匹配
5. **动作执行**：匹配的包被REJECT或ACCEPT

这个URL过滤系统设计精良，结合了应用层SNI识别和网络层iptables控制，实现了高效、灵活的Web访问控制功能。