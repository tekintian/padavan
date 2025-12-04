info->invert 的定义、赋值与处理逻辑

## 1. 定义位置
在 xt_sni_filter.c 文件的结构体中定义：
~~~c
struct xt_sni_info {
    char sni[SNI_MAX_LEN];
    uint16_t len;
    uint16_t invert;
};
~~~

## 2. 赋值方式
invert 参数是在用户空间通过 iptables 命令设置的
当用户在规则中使用 ! 符号时，invert 会被设置为 1
这个值通过 par->matchinfo 指针从用户空间传递到内核模块的 xt_sni_match 函数
在 netfilter 框架中，这是标准的参数传递机制

## 3. 处理逻辑
在代码中有两处关键的处理逻辑：

a) 错误处理路径
在多个错误处理点直接返回 info->invert：

~~~c
// 例如在偏移量验证失败时
if (payload_offset >= skb->len || payload_offset + 6 > skb->len) {
    DEBUGP("Invalid payload offset: %u, skb len: %u\n", payload_offset, skb->len);
    return info->invert;
}

// 在内存分配失败时
if (!tmp_buffer) {
    DEBUGP("Memory allocation failed\n");
    return info->invert;
}

// 在SNI提取失败时
if (sni_len < 0) {
    DEBUGP("Failed to extract SNI, allowing packet through\n");
    return info->invert;
}
~~~

b) 最终匹配结果计算
在成功提取SNI并进行匹配后的核心逻辑（第754-755行）：

~~~c
// 只有当明确匹配时才应用结果，否则默认放行
result = matched ? (matched ^ info->invert) : info->invert;
ENHANCED_DEBUG("Final match result (after invert): %s\n", result ? "true" : "false");

return result;
~~~

4. 工作原理详解
netfilter match模块返回值规则：

返回 true 表示数据包匹配规则条件，应执行规则动作（如DROP/ACCEPT）
返回 false 表示数据包不匹配规则条件，不执行规则动作

invert 参数的影响：

当 invert=0（默认）：正常匹配逻辑
当 invert=1（用户使用 ! 时）：匹配结果被反转
公式 matched ? (matched ^ info->invert) : info->invert 的含义：

如果 matched=true（找到SNI匹配）：
invert=0 时返回 true（匹配成功）
invert=1 时返回 false（匹配失败，即不匹配）
如果 matched=false（未找到SNI匹配）：
invert=0 时返回 false（不匹配）
invert=1 时返回 true（匹配成功，即不匹配时反而匹配）






## xt_sni_match函数的处理流程

1. **函数注册机制**：
   - xt_sni_match函数通过xt_match结构体注册到netfilter框架中
   - 在xt_sni_filter.c文件末尾，我们可以看到它被注册为"sni"匹配模块
   - 注册后，当iptables规则中使用-m sni参数时，这个函数会被调用

2. **返回值处理机制**：
   - xt_sni_match返回true：表示数据包匹配当前规则的条件
   - xt_sni_match返回false：表示数据包不匹配当前规则的条件
   - netfilter框架根据这个返回值来决定是否执行规则中指定的动作（如ACCEPT、DROP等）

3. **规则链处理流程**：
   - 当数据包到达netfilter钩子点时，系统会遍历对应的规则链
   - 对于每条规则，会依次执行规则中定义的所有匹配函数（包括xt_sni_match）
   - 只有当所有匹配函数都返回true时，才会执行该规则的目标动作
   - 如果数据包不匹配当前规则，系统会继续检查链中的下一条规则

4. **invert标志的影响**：
   - 当用户在规则中使用!符号时，info->invert被设置为1
   - xt_sni_match函数内部使用公式：`result = matched ? (matched ^ info->invert) : info->invert`
   - 这个公式会在匹配成功时反转结果（当invert=1时）

5. **与iptables的交互**：
   - iptables命令行工具负责解析用户规则并创建相应的内核规则结构
   - 当执行iptables规则时，内核中的netfilter框架会调用对应的匹配函数
   - 匹配函数的返回值直接影响数据包是否会被规则处理


