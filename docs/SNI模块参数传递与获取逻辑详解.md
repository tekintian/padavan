# SNI模块参数传递与获取逻辑详解

SNI模块的参数传递和获取逻辑遵循xtables扩展模块的标准工作机制，主要涉及以下几个核心部分：

## 1. 命令行选项定义

首先，模块需要定义支持的命令行选项，这是通过`sni_opts`结构体实现的：

```c
static const struct option sni_opts[] = {
    { .name = "str",      .has_arg = true,  .val = '1' },
    { .name = "invert",   .has_arg = false, .val = '2' },
    XT_GETOPT_TABLEEND,
};
```

- **name**: 命令行选项名称（如`--str`、`--invert`）
- **has_arg**: 是否需要参数（`true`表示需要）
- **val**: 选项的标识值，用于在解析函数中区分不同选项

## 2. 参数解析函数

xtables框架会将命令行参数传递给`sni_parse`函数，该函数负责解析选项并填充匹配结构：

```c
static int sni_parse(int c, char **argv, int invert, unsigned int *flags,
                    const void *entry, struct xt_entry_match **match)
{
    struct xt_sni_info *info = (struct xt_sni_info *)(*match)->data;
    
    switch (c) {
    case '1':  /* --str */
        // 解析URL模式参数
        break;
    case '2':  /* --invert */
        // 设置反转标志
        break;
    default:
        return 0;
    }
    
    return 1;
}
```

### 参数说明：
- **c**: 当前解析到的选项标识（对应`sni_opts`中的val字段）
- **argv**: 命令行参数数组，`argv[optind]`指向当前选项的参数值
- **invert**: xtables框架提供的反转标志（来自`!`符号）
- **flags**: 模块内部的标志位，用于跟踪已解析的选项
- **match**: 指向当前匹配项的指针，`match->data`是存储参数的结构体

## 3. URL模式解析流程（--str选项）

```c
case '1':  /* --str */
    if (*flags & 0x01)
        xtables_error(PARAMETER_PROBLEM, "Cannot specify --str twice");
    
    if (!argv[optind])
        xtables_error(PARAMETER_PROBLEM, "--str requires an argument");
    
    if (!validate_url_pattern(argv[optind]))
        xtables_error(PARAMETER_PROBLEM, "Invalid URL pattern format");
    
    /* 复制模式串 */
    strncpy(info->pattern, argv[optind], XT_SNI_MAX_PATTERN_SIZE - 1);
    info->pattern[XT_SNI_MAX_PATTERN_SIZE - 1] = '\0';
    info->pattern_len = strlen(info->pattern);
    
    /* 设置反转标志 - 优先级：--invert选项 > xtables的!符号 */
    if (*flags & 0x02) {
        /* 如果已经设置了--invert选项，则忽略!符号 */
        info->invert = 1;
    } else {
        /* 否则使用!符号的值 */
        info->invert = invert ? 1 : 0;
    }
    
    *flags |= 0x01;
    break;
```

### 处理步骤：
1. **检查重复选项**：确保`--str`只被指定一次
2. **验证参数存在**：确保`--str`后面有参数
3. **验证参数格式**：通过`validate_url_pattern`函数检查URL模式的有效性
4. **保存参数值**：将模式字符串复制到`info->pattern`字段
5. **设置反转标志**：根据`--invert`选项和`!`符号的优先级设置反转标志
6. **标记选项已解析**：设置`flags`中的0x01位

## 4. 反转标志处理（--invert选项）

```c
case '2':  /* --invert */
    /* 显式设置反转标志并记录标志位 */
    info->invert = 1;
    *flags |= 0x02;
    break;
```

### 处理步骤：
1. **设置反转标志**：将`info->invert`设置为1
2. **标记选项已解析**：设置`flags`中的0x02位

## 5. 参数验证

解析完成后，xtables框架会调用`sni_check`函数进行参数验证：

```c
static void sni_check(unsigned int flags)
{
    if (!(flags & 0x01))
        xtables_error(PARAMETER_PROBLEM, "URL filter SNI match requires --str");
    
    /* URL过滤模式不需要额外的参数检查 */
}
```

- **验证必须参数**：确保`--str`选项已经被解析（检查0x01标志位）

## 6. 参数初始化

在参数解析前，xtables框架会调用`sni_init`函数初始化匹配结构：

```c
static void sni_init(struct xt_entry_match *m)
{
    struct xt_sni_info *i = (struct xt_sni_info *) m->data;
    
    /* 初始化所有字段为默认值 */
    memset(i, 0, sizeof(struct xt_sni_info));
    
    /* 设置默认值 */
    i->invert = 0;
    i->wildcard_type = XT_SNI_MATCH_EXACT;
    i->pattern_len = 0;
    
    /* 确保字符串字段以NULL结尾 */
    i->pattern[0] = '\0';
    i->search_pattern[0] = '\0';
    i->ts_config = NULL;
}
```

- **初始化内存**：使用`memset`清零整个结构，避免未初始化内存
- **设置默认值**：为关键字段设置合理的默认值
- **确保字符串安全**：初始化字符串字段为空字符串

## 7. 与内核模块的交互

解析后的参数会被填充到`xt_sni_info`结构体中，这个结构体通过`xt_entry_match->data`传递给内核模块：

```c
struct xt_sni_info {
    char pattern[XT_SNI_MAX_PATTERN_SIZE];      /* 原始模式 */
    __u8  wildcard_type;                         /* 通配符类型 */
    __u8  invert;                                /* 反转标志 */
    __u8  reserved[2];                           /* 保留对齐 */
    
    char search_pattern[XT_SNI_MAX_PATTERN_SIZE]; /* 转换后的搜索模式 */
    __u32 pattern_len;                            /* 模式长度 */
    
    struct ts_config __attribute__((aligned(8))) *ts_config; /* 内核内部使用 */
};
```

- **用户态填写**：`pattern`、`invert`、`pattern_len`等字段由用户态模块填写
- **内核态处理**：`wildcard_type`、`search_pattern`等字段由内核模块在初始化时处理
- **内核内部使用**：`ts_config`字段由内核模块内部使用，用户态不需要处理

## 8. 修复的问题

我们之前的修改主要解决了以下参数处理中的问题：

1. **未初始化内存**：修复了`sni_init`函数，确保所有字段都被正确初始化
2. **参数冲突**：修复了`--invert`选项与xtables`!`符号的优先级问题
3. **字段匹配**：确保用户态和内核态使用的结构体字段完全匹配

## 总结

SNI模块的参数传递和获取逻辑遵循xtables扩展的标准流程，通过`parse`函数解析命令行选项，`check`函数验证参数，`init`函数初始化结构，并最终将参数传递给内核模块进行匹配。正确处理参数的传递和获取是确保模块稳定运行的关键。
        