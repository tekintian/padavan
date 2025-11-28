# MAC Filter Logic Test Demo

## 概述
这是一个用于验证Padavan路由器固件中MAC过滤功能算法逻辑的测试程序。该程序模拟了`include_mac_filter`函数的核心逻辑，包括拒绝模式和允许模式下的MAC地址过滤规则生成。

## 文件结构
```
demo/
├── test_mac_filter.c    # 测试程序源代码
├── test_mac_filter      # 编译后的可执行文件
└── readme.md           # 本说明文档
```

## 编译和运行

### 在macOS上编译
```bash
cd demo
gcc test_mac_filter.c -o test_mac_filter
```

### 运行测试
```bash
./test_mac_filter
```

## 测试功能

### 1. 拒绝模式测试（mac_filter_mode = 2）
**功能描述**：列表中的MAC地址在指定时间外被拒绝访问，其他设备允许。

**算法特点**：
- 按MAC地址分组管理时间规则
- 自动去重相同的时间规则
- 为每个MAC生成时间允许规则，最后添加DROP规则
- 无时间规则的MAC直接生成DROP规则

**测试数据**：
- MAC `11:22:33:44:55:66`：3个不同时间规则（含1个重复规则）
- MAC `aa:bb:cc:dd:ee:ff`：2个不同时间规则  
- MAC `22:33:44:55:66:77`：无时间规则
- 空MAC地址：应该被跳过

**预期输出**：每个MAC先输出时间允许规则，最后添加DROP规则

### 2. 允许模式测试（mac_filter_mode = 1）
**功能描述**：列表中的MAC地址在指定时间内被允许访问，其他设备拒绝。

**算法特点**：
- 为每个MAC生成允许规则
- 最后添加一条全局DROP规则
- 无时间规则的MAC生成无时间限制的允许规则

**测试数据**：
- MAC `11:22:33:44:55:66`：时间规则 `08001200`
- MAC `aa:bb:cc:dd:ee:ff`：时间规则 `14001800`
- MAC `22:33:44:55:66:77`：无时间规则

**预期输出**：每个MAC的允许规则，最后一条全局DROP规则

## 模拟函数说明

### 核心数据结构
```c
typedef struct {
    char mac[18];                    // MAC地址
    char time_rules[MAX_TIME_RULES][160];  // 时间规则数组
    int time_rule_count;            // 时间规则数量
    int has_drop_rule;              // DROP规则标记
} mac_entry_t;
```

### 模拟的NVRAM函数
- `nvram_get_int()`：获取整数配置值
- `nvram_safe_get()`：获取字符串配置值
- `mac_conv()`：MAC地址转换函数
- `timematch_conv()`：时间规则转换函数
- `logmessage()`：日志输出函数

### 全局数据模拟
- `macfilter_list_x[]`：MAC地址列表
- `macfilter_date_x[]`：日期规则列表  
- `macfilter_time_x[]`：时间规则列表
- `macfilter_num_x`：规则总数

## 输出格式

### 拒绝模式输出示例
```
-A maclist -m mac --mac-source 11:22:33:44:55:66 --timestart 08001200 --timestop 08001200 -j RETURN
-A maclist -m mac --mac-source 11:22:33:44:55:66 --timestart 14001800 --timestop 14001800 -j RETURN
-A maclist -m mac --mac-source 11:22:33:44:55:66 -j DROP
[MAC Filter] INFO: Processed 3 unique MACs with 7 total rules in deny mode
```

### 允许模式输出示例
```
-A maclist -m mac --mac-source 11:22:33:44:55:66 --timestart 08001200 --timestop 08001200 -j RETURN
-A maclist -m mac --mac-source aa:bb:cc:dd:ee:ff --timestart 14001800 --timestop 14001800 -j RETURN
-A maclist -m mac --mac-source 22:33:44:55:66:77 -j RETURN
-A maclist -j DROP
[MAC Filter] INFO: Processed 3 MAC entries in allow mode
```

## 测试验证要点

1. **时间规则去重**：相同MAC的相同时间规则应该只出现一次
2. **MAC分组**：同一MAC的所有时间规则应该集中在一起
3. **DROP规则位置**：拒绝模式下，每个MAC的DROP规则应该在所有时间规则之后
4. **空MAC处理**：空MAC地址应该被正确跳过
5. **无时间规则处理**：无时间规则的MAC应该生成相应的默认规则

## 与实际代码的区别

本测试程序模拟了Padavan固件中`firewall_ex.c`文件的`include_mac_filter`函数逻辑，但做了以下简化：

1. 移除了实际的NVRAM访问，使用内存数组模拟
2. 简化了时间规则解析逻辑
3. 输出发送到stdout而不是iptables配置文件
4. 移除了debug日志输出，仅保留关键信息

## 使用建议

1. 修改测试数据时，请保持数据格式的完整性
2. 新增测试场景时，参考现有的`init_test_data()`和`test_allow_mode()`函数结构
3. 验证算法修改时，重点关注时间规则去重和MAC分组逻辑
4. 输出结果可以直接与预期iptables规则进行对比验证


这个readme.md文档详细说明了测试程序的使用方法、功能特点、算法逻辑和验证要点，方便后续开发和测试工作。
