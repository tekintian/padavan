# iptables string 扩展技术文档

## 1. 概述

iptables string 扩展是一个用于在网络数据包中匹配特定字符串的功能模块。该模块允许管理员根据数据包有效载荷中包含的特定文本内容来过滤网络流量，提供了基于内容的防火墙能力。

## 2. 技术实现

### 2.1 核心数据结构

string 扩展的核心数据结构在 <mcfile name="xt_string.h" path="trunk/user/iptables/iptables-1.8.7/include/linux/netfilter/xt_string.h"></mcfile> 中定义：

```c
struct xt_string_info {
	__u16 from_offset;        // 搜索起始偏移量
	__u16 to_offset;          // 搜索结束偏移量
	char  algo[XT_STRING_MAX_ALGO_NAME_SIZE];  // 匹配算法名称
	char  pattern[XT_STRING_MAX_PATTERN_SIZE]; // 要匹配的模式串
	__u8  patlen;             // 模式串长度
	union {
		struct {
			__u8 invert;      // 旧版本的反转标志
		} v0;

		struct {
			__u8 flags;       // 新版本的标志位
		} v1;
	} u;

	/* 内核内部使用 */
	struct ts_config __attribute__((aligned(8))) *config;
};
```

### 2.2 常量定义

```c
#define XT_STRING_MAX_PATTERN_SIZE 128         // 最大模式串长度
#define XT_STRING_MAX_ALGO_NAME_SIZE 16        // 最大算法名称长度

enum {
	XT_STRING_FLAG_INVERT      = 0x01,  // 反转匹配结果
	XT_STRING_FLAG_IGNORECASE  = 0x02   // 忽略大小写
};
```

### 2.3 命令行选项处理

在 <mcfile name="libxt_string.c" path="trunk/user/iptables/iptables-1.8.7/extensions/libxt_string.c"></mcfile> 中，定义了以下命令行选项：

```c
static const struct xt_option_entry string_opts[] = {
	{.name = "from", .id = O_FROM, .type = XTTYPE_UINT16,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, from_offset)},
	{.name = "to", .id = O_TO, .type = XTTYPE_UINT16,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, to_offset)},
	{.name = "algo", .id = O_ALGO, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_PUT, XTOPT_POINTER(s, algo)},
	{.name = "string", .id = O_STRING, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT, .excl = F_HEX_STRING},
	{.name = "hex-string", .id = O_HEX_STRING, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT, .excl = F_STRING},
	{.name = "icase", .id = O_ICASE, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};
```

### 2.4 核心功能实现

#### 2.4.1 字符串解析

模块提供了两种字符串解析方式：普通字符串和十六进制字符串：

1. **普通字符串解析** (<mcsymbol name="parse_string" filename="libxt_string.c" path="trunk/user/iptables/iptables-1.8.7/extensions/libxt_string.c" startline="89" type="function"></mcsymbol>)：
   - 检查字符串长度是否超过最大限制
   - 将字符串复制到模式缓冲区
   - 设置模式长度

2. **十六进制字符串解析** (<mcsymbol name="parse_hex_string" filename="libxt_string.c" path="trunk/user/iptables/iptables-1.8.7/extensions/libxt_string.c" startline="101" type="function"></mcsymbol>)：
   - 支持混合文本和十六进制格式
   - 十六进制数据使用 `|` 分隔
   - 支持转义字符 `\`
   - 解析并存储转换后的二进制模式

#### 2.4.2 选项解析与处理

<mcsymbol name="string_parse" filename="libxt_string.c" path="trunk/user/iptables/iptables-1.8.7/extensions/libxt_string.c" startline="172" type="function"></mcsymbol> 函数负责处理用户指定的各种选项：

- 根据不同选项ID分发到相应的处理逻辑
- 处理反转匹配选项 (! --string 或 ! --hex-string)
- 处理大小写不敏感选项 (--icase)
- 根据内核版本兼容性设置相应标志

#### 2.4.3 输出格式化

模块提供了两个格式化输出函数：

1. <mcsymbol name="print_string" filename="libxt_string.c" path="trunk/user/iptables/iptables-1.8.7/extensions/libxt_string.c" startline="249" type="function"></mcsymbol>：输出普通字符串，处理引号和转义
2. <mcsymbol name="print_hex_string" filename="libxt_string.c" path="trunk/user/iptables/iptables-1.8.7/extensions/libxt_string.c" startline="238" type="function"></mcsymbol>：以十六进制格式输出包含非打印字符的字符串

<mcsymbol name="is_hex_string" filename="libxt_string.c" path="trunk/user/iptables/iptables-1.8.7/extensions/libxt_string.c" startline="228" type="function"></mcsymbol> 函数用于检测字符串是否需要以十六进制格式输出。

### 2.5 版本兼容性

模块支持两个版本的接口：

```c
static struct xtables_match string_mt_reg[] = {
	{
		.name          = "string",
		.revision      = 0,
		/* 版本0特定配置 */
	},
	{
		.name          = "string",
		.revision      = 1,
		/* 版本1特定配置 */
	},
};
```

版本1增加了对 `--icase` 选项的支持，通过 flags 字段替换了旧版本的 invert 字段。

## 3. 特性与参数

### 3.1 必需参数

- `--algo <算法名称>`: 指定字符串匹配算法，支持 "bm"(Boyer-Moore) 和 "kmp"(Knuth-Morris-Pratt) 等算法
- `--string "字符串"` 或 `--hex-string "|十六进制数据|"`: 指定要匹配的字符串内容

### 3.2 可选参数

- `--from <偏移量>`: 指定开始搜索的数据包偏移量
- `--to <偏移量>`: 指定停止搜索的数据包偏移量
- `--icase`: 匹配时忽略大小写（仅在版本1接口中支持）
- `!` 前缀: 反转匹配结果（不匹配指定字符串）

### 3.3 匹配算法

string 扩展支持多种字符串匹配算法，最常用的包括：

1. **Boyer-Moore (bm)**: 通常是最快的字符串匹配算法，特别适合较长模式串
2. **Knuth-Morris-Pratt (kmp)**: 另一种高效的字符串匹配算法

## 4. 使用示例

### 4.1 基本文本匹配

```bash
# 阻止包含 "virus" 字符串的入站流量
iptables -A INPUT -m string --algo bm --string "virus" -j DROP

# 允许包含 "allowed_content" 字符串的出站流量
iptables -A OUTPUT -m string --algo bm --string "allowed_content" -j ACCEPT
```

### 4.2 十六进制匹配

```bash
# 匹配特定二进制序列（例如匹配 HTTP GET 请求的部分特征）
iptables -A FORWARD -m string --algo bm --hex-string "|474554202F|" -j LOG
```

### 4.3 使用高级选项

```bash
# 忽略大小写匹配
# 注意：这需要内核支持版本1的string模块接口
iptables -A INPUT -m string --algo bm --string "VIRUS" --icase -j DROP

# 只在数据包的前100字节内搜索
iptables -A INPUT -m string --algo bm --from 0 --to 100 --string "malicious" -j DROP

# 反向匹配（不包含特定字符串）
iptables -A FORWARD ! -m string --algo bm --string "content-type:" -j LOG
```

### 4.4 结合其他匹配条件

```bash
# 阻止来自特定IP地址且包含特定字符串的流量
iptables -A INPUT -s 192.168.1.100 -m string --algo bm --string "malware" -j DROP

# 限制HTTP流量中的特定内容
iptables -A FORWARD -p tcp --dport 80 -m string --algo bm --string "bad_content" -j DROP
```

## 5. 实现细节

### 5.1 内核与用户空间交互

用户空间工具通过 `struct xt_string_info` 结构将匹配参数传递给内核模块。内核使用 `ts_config` 结构进行实际的字符串匹配操作，这个结构由文本搜索API内部管理。

### 5.2 字符串长度限制

模式串的最大长度限制为128字节（`XT_STRING_MAX_PATTERN_SIZE`），超过此限制的字符串将被拒绝。

### 5.3 特殊字符处理

对于包含非打印字符的字符串，模块会自动检测并以十六进制格式输出，以确保正确保存和恢复规则。

## 6. 注意事项

1. **性能影响**: 字符串匹配是CPU密集型操作，可能对网络性能产生显著影响，特别是在高流量环境中
2. **算法选择**: Boyer-Moore算法通常比其他算法性能更好，但具体性能取决于模式串和数据特征
3. **版本兼容性**: `--icase` 选项仅在内核支持string模块版本1接口时可用
4. **安全考虑**: 字符串匹配可能被规避，不应用作唯一的安全措施

通过结合这些功能，iptables string扩展提供了强大的基于内容的数据包过滤能力，可以用于各种网络安全和流量控制场景。

