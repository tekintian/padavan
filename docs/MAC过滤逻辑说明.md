# MAC过滤模式代码验证文档

## 概述
本文档基于 `trunk/user/rc/src/firewall_ex.c` 代码验证MAC过滤的真实实现逻辑。

## 核心变量说明

### NVRAM配置变量
- `macfilter_enable_x`: MAC过滤模式值
  - `0`: 关闭MAC过滤
  - `1`: 允许模式
  - `2`: 拒绝模式
- `macfilter_num_x`: 规则列表中的MAC地址数量
- `macfilter_list_x[i]`: 第i个MAC地址
- `macfilter_date_x[i]`: 第i个规则的日期配置
- `macfilter_time_x[i]`: 第i个规则的时间配置
- `fw_mac_drop`: 允许模式下的额外拒绝选项

### 链名称定义
- `IPT_CHAIN_NAME_MAC_LIST`: MAC过滤链 (maclist)

---

## 拒绝模式 (macfilter_enable_x = 2)

### 代码位置
- 函数: `include_mac_filter()` (第685-830行)
- FORWARD链处理 (第1590-1677行)
- INPUT链处理 (第1342-1356行, 第2079-2087行)

### 核心逻辑

#### 1. maclist链规则生成 (695-794行)

```c
if (mac_filter_mode == 2) {
    // 第一轮：收集所有MAC及其时间规则
    for (i = 0; i < total_rules; i++) {
        // 查找或创建MAC条目
        // 收集每个MAC的时间规则
    }
    
    // 第二轮：为每个MAC生成规则（先时间规则，最后DROP规则）
    for (int j = 0; j < mac_count; j++) {
        // 生成所有时间允许规则
        for (int r = 0; r < entry->time_rule_count; r++) {
            fprintf(fp, "-A %s -m mac --mac-source %s%s -j RETURN\n",
                    dtype, entry->mac, entry->time_rules[r]);
        }
        
        // 最后添加DROP规则（确保是该MAC的最后一条规则）
        if (entry->time_rule_count > 0) {
            fprintf(fp, "-A %s -m mac --mac-source %s -j %s\n",
                    dtype, entry->mac, logdrop);
        }
    }
}
```

**关键特点**:
- 按MAC地址分组处理
- 每个MAC先添加时间允许规则 (`RETURN`)
- 最后添加无条件 `DROP` 规则
- 只处理规则列表中的MAC

#### 2. FORWARD链跳转逻辑 (1590-1677行)

```c
if (mac_filter_mode == 2) {
    // 拒绝模式：只将规则列表中的设备重定向到maclist链
    foreach_x("macfilter_num_x") {
        g_buf_init();
        filter_mac = mac_conv("macfilter_list_x", i, mac_buf);
        if (*filter_mac) {
            fprintf(fp, "-A %s -i %s -m mac --mac-source %s -j %s\n",
                   dtype, lan_if, filter_mac, IPT_CHAIN_NAME_MAC_LIST);
        }
    }
}
```

**关键特点**:
- **只将规则列表中的设备**跳转到 `maclist` 链
- 规则列表外的设备**不经过MAC过滤检查**

#### 3. 生成的iptables规则示例

假设规则列表中有一个MAC地址 `AA:BB:CC:DD:EE:FF`, 时间配置为工作日 09:00-18:00:

```bash
# maclist链
-A maclist -m mac --mac-source AA:BB:CC:DD:EE:FF -m time --timestart 09:00:00 --timestop 18:00:00 --weekdays Mon,Tue,Wed,Thu,Fri --kerneltz -j RETURN
-A maclist -m mac --mac-source AA:BB:CC:DD:EE:FF -j LOGDROP

# FORWARD链
-A FORWARD -i br0 -m mac --mac-source AA:BB:CC:DD:EE:FF -j maclist
-A FORWARD -m state --state ESTABLISHED,RELATED -j ACCEPT
-A FORWARD -i br0 -j ACCEPT
```

### 流量处理流程

#### 规则列表中的设备 (例如: AA:BB:CC:DD:EE:FF)

1. 进入FORWARD链
2. 匹配 `-i br0 -m mac --mac-source AA:BB:CC:DD:EE:FF` → 跳转到maclist链
3. 在maclist链中:
   - **在时间窗内** (09:00-18:00, 工作日): 匹配时间规则 → `RETURN` → 返回FORWARD链继续处理
   - **超出时间窗**: 不匹配时间规则, 匹配 `DROP` 规则 → `LOGDROP` → 拒绝
4. 返回FORWARD链后继续后续规则处理

#### 规则列表外的设备 (例如: 11:22:33:44:55:66)

1. 进入FORWARD链
2. **不匹配** `-i br0 -m mac --mac-source AA:BB:CC:DD:EE:FF` → 跳过maclist链
3. 继续后续规则:
   - 匹配 `ESTABLISHED,RELATED` → `ACCEPT`
   - 匹配 `-i br0 -j ACCEPT` → 允许访问网络
4. **完全不受MAC过滤限制,直接允许访问网络**

### 逻辑总结

**拒绝模式 = 选择性拒绝**

- **规则列表中的设备**: 需要通过时间检查,不满足则拒绝
- **规则列表外的设备**: **完全不受限制,直接允许访问网络**

---

## 允许模式 (macfilter_enable_x = 1)

### 核心逻辑

#### 1. maclist链规则生成 (795-823行)

```c
else {
    // 允许模式: 列表中的设备允许，其他设备拒绝
    ftype = "RETURN";
    mac_num = 0;
    
    for (i = 0; i < total_rules; i++) {
        filter_mac = mac_conv("macfilter_list_x", i, mac_buf);
        if (!*filter_mac) continue;
        
        sprintf(nv_date, "macfilter_date_x%d", i);
        sprintf(nv_time, "macfilter_time_x%d", i);
        timematch_conv(mac_timematch, nv_date, nv_time);
        
        fprintf(fp, "-A %s -m mac --mac-source %s%s -j %s\n",
                dtype, filter_mac, mac_timematch, ftype);
        mac_num++;
    }
    
    if (mac_num > 0) {
        // 允许模式: 列表外的设备拒绝
        fprintf(fp, "-A %s -j %s\n", dtype, logdrop);
    }
}
```

#### 2. FORWARD链跳转逻辑 (1673-1676行)

```c
else {
    // 允许模式：所有LAN设备都进入maclist链
    fprintf(fp, "-A %s -i %s -j %s\n", dtype, lan_if, IPT_CHAIN_NAME_MAC_LIST);
}
```

#### 3. 生成的iptables规则示例

```bash
# maclist链
-A maclist -m mac --mac-source AA:BB:CC:DD:EE:FF -m time --timestart 09:00:00 --timestop 18:00:00 --weekdays Mon,Tue,Wed,Thu,Fri --kerneltz -j RETURN
-A maclist -j LOGDROP

# FORWARD链
-A FORWARD -i br0 -j maclist
-A FORWARD -m state --state ESTABLISHED,RELATED -j ACCEPT
```

### 流量处理流程

#### 规则列表中的设备 (AA:BB:CC:DD:EE:FF)

1. 进入FORWARD链, 所有LAN设备跳转到maclist链
2. 在maclist链中:
   - **在时间窗内**: 匹配MAC和时间规则 → `RETURN` → 返回FORWARD链
   - **超出时间窗**: 不匹配任何规则 → 匹配最后的 `LOGDROP` → 拒绝
3. 返回FORWARD链继续处理

#### 规则列表外的设备 (11:22:33:44:55:66)

1. 进入FORWARD链, 所有LAN设备跳转到maclist链
2. 在maclist链中:
   - 不匹配任何MAC规则
   - 匹配最后的 `-A maclist -j LOGDROP` → **拒绝**
3. **规则外的设备被拒绝访问**

### 逻辑总结

**允许模式 = 选择性允许**

- **规则列表中的设备**: 在时间窗内允许, 超出时间窗拒绝
- **规则列表外的设备**: **全部拒绝访问网络**

---

## 两种模式对比

| 特性 | 允许模式 (1) | 拒绝模式 (2) |
|------|-------------|-------------|
| **策略** | 白名单模式 | 黑名单模式 |
| **规则列表中的设备** | 时间窗内允许, 超时拒绝 | 时间窗内允许, 超时拒绝 |
| **规则列表外的设备** | **全部拒绝** | **全部允许** |
| **链跳转范围** | 所有LAN设备都进入maclist链 | 只有规则列表中的设备进入maclist链 |
| **maclist链末尾规则** | `-j DROP` (拒绝列表外设备) | 无末尾DROP规则 (由主链ACCEPT) |
| **主链默认策略** | 无特殊处理 | `-i br0 -j ACCEPT` (允许规则外设备) |

---

## 时间规则说明

时间规则配置:
- `macfilter_date_x[i]`: 7位字符串, 表示周日到周六是否启用 (`1`=启用, `0`=禁用)
  - `"1111111"`: 每天
  - `"0000000"`: 不启用
- `macfilter_time_x[i]`: 8位字符串, 前四位开始时间, 后四位结束时间
  - `"00002359"`: 全天
  - `"09301800"`: 09:30-18:00

时间规则转换函数: `timematch_conv()` (第315-386行)

---

## 与URL过滤的协同处理

当同时启用MAC过滤和URL过滤时 (1556-1678行):

### MAC允许模式 + URL过滤 (1558-1589行)

```c
if (mac_filter_mode == 1) {
    // MAC允许模式：先MAC过滤，再URL过滤
    fprintf(fp, "-A %s -i %s -j %s\n", dtype, lan_if, IPT_CHAIN_NAME_MAC_LIST);
    // URL过滤只对MAC允许的设备处理
    include_webstr_filter(fp);
}
```

### MAC拒绝模式 + URL过滤 (1590-1628行)

```c
else {
    // MAC拒绝模式：先MAC过滤规则设备，再URL过滤
    foreach_x("macfilter_num_x") {
        filter_mac = mac_conv("macfilter_list_x", i, mac_buf);
        if (*filter_mac) {
            fprintf(fp, "-A %s -i %s -m mac --mac-source %s -j %s\n",
                   dtype, lan_if, filter_mac, IPT_CHAIN_NAME_MAC_LIST);
        }
    }
    // URL过滤对非MAC规则设备处理
    include_webstr_filter(fp);
}
```

---

## 调试日志

代码中包含详细的调试日志:

```c
logmessage("MAC Filter", "DEBUG: Processing MAC %s with %d time rules",
           entry->mac, entry->time_rule_count);
logmessage("MAC Filter", "DEBUG: Added time rule for MAC %s: %s",
           entry->mac, entry->time_rules[r]);
logmessage("MAC Filter", "DEBUG: Added DROP rule for MAC %s", entry->mac);
logmessage("MAC Filter", "INFO: Processed %d unique MACs with %d total rules in deny mode", 
           mac_count, mac_num);
```

可通过 `logread` 命令查看系统日志:

```bash
logread | grep "MAC Filter"
```

---

## 验证命令

查看实际生成的iptables规则:

```bash
# 查看maclist链
iptables -t filter -L maclist -n -v

# 查看FORWARD链
iptables -t filter -L FORWARD -n -v

# 查看INPUT链
iptables -t filter -L INPUT -n -v
```

---

## 总结

**拒绝模式的核心逻辑**:
1. 只对规则列表中的设备进行MAC检查
2. 规则列表中的设备需要满足时间条件, 不满足则拒绝
3. 规则列表外的设备**完全不受限制, 直接允许访问网络**

**允许模式的核心逻辑**:
1. 对所有LAN设备进行MAC检查
2. 规则列表中的设备在时间窗内允许
3. 规则列表外的设备**全部拒绝访问网络**

两种模式互为互补:
- 允许模式: 严格的访问控制, 只允许指定的设备
- 拒绝模式: 宽松的访问控制, 只限制指定的设备

---

**文档版本**: v1.0  
**代码版本**: 基于 `trunk/user/rc/src/firewall_ex.c`  
**验证日期**: 2026-01-01
