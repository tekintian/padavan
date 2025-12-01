# SNI模块修复说明

## 问题描述

URL过滤功能中的HTTPS域名过滤不工作，具体表现为：
- HTTP过滤（webstr模块）工作正常
- HTTPS过滤（SNI模块）完全无效
- 所有SNI规则计数器为0，无法匹配任何流量

## 根本原因分析

经过深入分析发现SNI模块存在以下关键问题：

1. **TCP头长度计算错误**
   - 原代码使用 `sizeof(struct tcphdr)` 计算TCP头长度
   - 正确做法应该是使用 `tcph->doff * 4`（考虑TCP选项）

2. **数据长度检查过于严格**
   - 原代码要求最小6字节，实际TLS记录头最小为5字节

3. **缺少调试信息**
   - 无法诊断模块是否被调用
   - 难以定位包解析问题

4. **内存管理问题**
   - 错误处理路径中存在use-after-free风险

## 修复内容

### 1. 修复TCP头长度计算
```c
// 修复前
payload_offset = iph->ihl * 4 + sizeof(struct tcphdr);
data_len = ntohs(iph->tot_len) - iph->ihl * 4 - sizeof(struct tcphdr);

// 修复后  
tcph = skb_header_pointer(skb, iph->ihl * 4, sizeof(_tcph), &_tcph);
if (!tcph) {
    DEBUGP("Failed to get TCP header\n");
    return false;
}
payload_offset = iph->ihl * 4 + tcph->doff * 4;
data_len = ntohs(iph->tot_len) - payload_offset;
```

### 2. 放宽数据长度检查
```c
// 修复前
if (data_len < 6) { /* TLS记录头最小长度 */

// 修复后
if (data_len < 5) { /* TLS记录头最小长度 */
```

### 3. 添加调试信息
```c
DEBUGP("TLS record: type=%u, data_len=%u, payload_offset=%d\n", 
       record_type, data_len, payload_offset);
DEBUGP("TLS ClientHello detected\n");
```

### 4. 改进错误处理
```c
if (sni_len < 0) {
    DEBUGP("Failed to extract SNI\n");
    DEBUGP("Data length: %u, buffer size: %zu\n", data_len, buffer_size);
    if (buffer_size >= 6) {
        DEBUGP("First 6 bytes: %02x %02x %02x %02x %02x %02x\n", 
               tmp_buffer[0], tmp_buffer[1], tmp_buffer[2], tmp_buffer[3], tmp_buffer[4], tmp_buffer[5]);
    }
    kfree(tmp_buffer);
    return false;
}
```

## 文件修改

- `trunk/linux-4.4.x/net/netfilter/xt_sni_filter.c` - SNI模块核心修复

## 测试验证

### 1. 编译测试
```bash
# 推送到GitHub后，CI会自动编译
git push origin dev
```

### 2. 功能测试
使用提供的测试脚本：
```bash
# 在路由器上执行
/tmp/test_sni_fix.sh
```

### 3. 预期结果
- SNI规则计数器应该大于0
- 内核日志中应该看到SNI匹配信息
- HTTPS域名过滤应该生效

## 临时解决方案

在修复版本编译完成前，可以使用临时方案：

1. **HTTP过滤** - 使用webstr模块，完全工作
2. **HTTPS过滤** - 使用IP地址阻止（有限效果）

临时脚本：`final_url_fix.sh`

## 长期解决方案

1. 应用SNI模块修复
2. 重新编译固件
3. 刷入修复后的固件
4. 验证HTTPS域名过滤功能

## 技术细节

### SNI（Server Name Indication）原理
- SNI是TLS协议的扩展
- 在TLS握手阶段以明文传输域名
- 允许服务器在同一个IP上托管多个HTTPS站点

### 为什么原来的SNI模块不工作
1. TCP头长度计算错误导致包解析失败
2. 数据长度检查过于严格拒绝有效包
3. 缺少调试信息难以发现问题

### 修复后的改进
1. 正确解析TCP包，包括有选项的情况
2. 合理的数据长度验证
3. 详细的调试日志便于问题诊断
4. 更好的错误处理和内存管理

## 验证步骤

1. **编译验证**：确保固件编译成功
2. **安装验证**：确认SNI符号存在
3. **功能验证**：测试HTTPS域名过滤
4. **性能验证**：确保不影响正常网络性能

## 相关文件

- `test_sni_fix.sh` - SNI修复测试脚本
- `final_url_fix.sh` - 临时工作方案
- `working_url_filter.sh` - HTTP过滤方案
- `debug_sni_packets.sh` - 调试脚本

## 注意事项

1. 修复需要重新编译内核模块
2. 建议先在测试环境验证
3. 保留原固件备份
4. 监控网络性能影响

---

**修复完成日期**: 2025-12-01  
**修复版本**: dev分支 commit 681e7d6ac8  
**测试状态**: 待编译验证