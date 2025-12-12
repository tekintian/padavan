# Textsearch Demo

这个演示程序展示了从 Linux 内核的 iptables string 模块中提取的 textsearch 业务逻辑。

## 功能特性

- **KMP (Knuth-Morris-Pratt) 算法**: 线性时间字符串匹配算法
- **Boyer-Moore 算法**: 高效的字符串搜索算法
- **大小写不敏感搜索**: 支持 ignorecase 标志
- **性能对比**: 提供算法性能基准测试
- **简化用户空间实现**: 移除了内核相关的代码，适用于用户空间程序

## 文件结构

```
textsearch/
├── README.md              # 本文件
├── Makefile               # 编译配置
├── textsearch.h           # textsearch 框架头文件
├── textsearch.c           # textsearch 框架实现
├── ts_kmp.c              # KMP 算法实现
├── ts_bm.c               # Boyer-Moore 算法实现
└── test_textsearch.c      # 测试演示程序
```

## 编译和运行

### 编译

```bash
make
```

### 运行测试

```bash
make test
# 或者直接运行
./test_textsearch
```

### 清理

```bash
make clean
```

## 算法说明

### KMP 算法 (Knuth-Morris-Pratt)

- **时间复杂度**: O(n + m)，其中 n 是文本长度，m 是模式长度
- **特点**: 预处理阶段构建前缀表，搜索阶段不会回退
- **适用场景**: 适合处理重复字符较多的文本

### Boyer-Moore 算法

- **时间复杂度**: 平均 O(n/m)，最坏 O(n*m)
- **特点**: 从右向左匹配，使用坏字符和好后缀规则跳过更多字符
- **适用场景**: 适合大模式匹配，平均性能优异

## API 使用示例

```c
#include "textsearch.h"

// 初始化算法
init_kmp();  // 或 init_bm()

// 准备搜索配置
struct ts_config *conf = textsearch_prepare("kmp", "pattern", strlen("pattern"), 0, 0);

// 执行搜索
struct ts_state state;
unsigned int pos = textsearch_find_continuous(conf, &state, text, strlen(text));

if (pos != UINT_MAX) {
    printf("Found at position %u\n", pos);
} else {
    printf("Not found\n");
}

// 清理资源
textsearch_destroy(conf);
exit_kmp();  // 或 exit_bm()
```

## 测试内容

演示程序包含以下测试：

1. **基础功能测试**
   - 多种模式匹配测试
   - 大小写敏感/不敏感测试
   - 不存在的模式测试

2. **性能基准测试**
   - 10000次迭代的性能对比
   - 微秒级精度计时
   - 算法间的性能对比分析

3. **实际场景测试**
   - 英文文本匹配
   - 常见关键词搜索
   - 性能统计数据

## 性能参考

在标准测试文本上的典型性能：

- **KMP**: 稳定的线性性能，适合重复字符多的场景
- **Boyer-Moore**: 在大多数情况下更快，特别是长模式匹配

## 原始代码来源

本演示代码基于以下原始文件：

- `trunk/user/iptables/iptables-1.8.7/extensions/libxt_string.c` - iptables string 模块
- `trunk/linux-4.4.x/include/linux/textsearch.h` - textsearch 框架头文件
- `trunk/linux-4.4.x/lib/ts_kmp.c` - KMP 算法实现
- `trunk/linux-4.4.x/lib/ts_bm.c` - Boyer-Moore 算法实现

## 注意事项

1. 这是简化版本，移除了内核相关的内存管理和模块加载机制
2. 使用标准 C 库函数替代内核函数
3. 适用于用户空间程序，不适用于内核环境
4. 性能可能与原始内核实现略有差异

## 扩展可能

- 添加更多字符串搜索算法（如 Rabin-Karp）
- 支持正则表达式匹配
- 添加多线程搜索支持
- 实现流式搜索接口