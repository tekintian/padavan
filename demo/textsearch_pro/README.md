# TextSearch Pro - Enhanced Text Search with Wildcard Support

这是原版 textsearch 框架的增强版本，新增了通配符模式搜索功能。

## 新增功能

### 通配符支持

支持两种通配符：
- `*` - 匹配任意长度字符串（包括空字符串）
- `?` - 匹配单个字符

### 编译和使用

```bash
# 编译完整测试程序
make

# 运行完整测试
make test

# 编译简单通配符测试
make simple

# 运行简单通配符测试
make simple

# 清理编译文件
make clean
```

## 通配符模式示例

| 模式 | 说明 | 匹配示例 |
|------|------|----------|
| `*` | 匹配所有内容 | "Hello World" |
| `Hello*` | 以 "Hello" 开头 | "Hello World", "Hello there" |
| `*ing` | 以 "ing" 结尾 | "matching", "searching" |
| `*is*` | 包含 "is" | "This is", "misery" |
| `H?llo` | H?llo，? 匹配单个字符 | "Hello", "Hillo" |
| `Th?s ?s *` | 复合通配符 | "This is a test" |

## API 使用

### 初始化算法
```c
#include "textsearch.h"

// 初始化通配符算法
init_wildcard();

// 准备搜索配置（必须包含 TS_WILDCARD 标志）
struct ts_config *conf = textsearch_prepare("wildcard", 
                                          pattern, 
                                          strlen(pattern), 
                                          0, 
                                          TS_WILDCARD);

// 执行搜索
struct ts_state state;
unsigned int pos = textsearch_find_continuous(conf, &state, text, strlen(text));

// 清理资源
textsearch_destroy(conf);
exit_wildcard();
```

### 结合大小写不敏感
```c
// 同时支持通配符和大小写不敏感
int flags = TS_WILDCARD | TS_IGNORECASE;
struct ts_config *conf = textsearch_prepare("wildcard", 
                                          pattern, 
                                          strlen(pattern), 
                                          0, 
                                          flags);
```

## 性能对比

基于100,000次迭代的详细性能测试结果（详见 `PERFORMANCE_REPORT.md`）：

### 普通字符串搜索性能
| 模式 | KMP | Boyer-Moore | Wildcard | 推荐算法 |
|------|-----|-------------|----------|----------|
| "performance" | 0.352μs | **0.178μs** | 1.709μs | Boyer-Moore |
| "Pattern" | 4.158μs | **2.704μs** | 4.648μs | Boyer-Moore |
| "xyz"(不存在) | 4.235μs | 2.994μs | **2.977μs** | Wildcard |

### 通配符搜索性能
| 模式 | Wildcard | 说明 |
|------|----------|------|
| "a*gorithm" | 1.079μs | 前缀匹配 |
| "*matching*" | 3.689μs | 包含匹配 |
| "????????" | 0.056μs | 固定长度匹配（最快） |

### 核心结论
1. **Boyer-Moore**: 普通字符串搜索的最佳选择，平均比KMP快30-40%
2. **KMP**: 性能稳定，适用于各种场景
3. **Wildcard**: 通配符功能的唯一选择，在某些特定模式中表现优异

## 支持的算法

1. **KMP** - Knuth-Morris-Pratt 算法（不支持通配符）
2. **BM** - Boyer-Moore 算法（不支持通配符）  
3. **WILDCARD** - 通配符算法（支持 * 和 ?）

## 实现细节

### 通配符算法特点：
- 支持标准的 shell 风格通配符
- 自动合并连续的 `*` 字符
- 支持大小写敏感/不敏感搜索
- 高效的回溯匹配算法
- 预处理模式以提高搜索效率

### 兼容性：
- 完全兼容原版 textsearch API
- 可以与现有的 KMP 和 BM 算法并存
- 支持 TS_IGNORECASE 标志
- 保持相同的内存管理和错误处理机制

## 文件结构

```
textsearch_pro/
├── textsearch.h          # 核心头文件（包含通配符标志）
├── textsearch.c          # 核心框架实现
├── ts_kmp.c            # KMP 算法实现
├── ts_bm.c             # Boyer-Moore 算法实现
├── ts_wildcard.c       # 新增：通配符算法实现
├── test_textsearch.c   # 完整测试程序
├── test_wildcard_simple.c # 简单通配符测试
├── Makefile            # 构建脚本
└── README.md           # 本文档
```