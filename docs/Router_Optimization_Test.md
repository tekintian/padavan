# SNI Router Optimization Test Plan

## 测试目标

验证新的路由器优化算法是否提升了 SNI 匹配性能，同时确保稳定性。

## 测试环境

- 路由器型号：Padavan 固件
- 内核版本：Linux 4.4.x
- 测试工具：iptables SNI 模块

## 测试场景

### 1. 精确域名匹配 (最高性能)
```bash
iptables -A OUTPUT -p tcp --dport 443 -m sni --sni "google.com" --algo router -j ACCEPT
```
**预期效果**：比 BM 算法快 30-50%

### 2. 子域名匹配
```bash
iptables -A OUTPUT -p tcp --dport 443 -m sni --sni "*.google.com" --algo router -j ACCEPT
```
**预期效果**：比 BM 算法快 20-40%

### 3. 包含匹配
```bash
iptables -A OUTPUT -p tcp --dport 443 -m sni --sni "video" --algo router -j ACCEPT
```
**预期效果**：与 BM 算法相当或略快

### 4. 大小写不敏感匹配
```bash
iptables -A OUTPUT -p tcp --dport 443 -m sni --sni "Google.com" --algo router --icase -j ACCEPT
```
**预期效果**：正确处理大小写

## 性能基准测试

### 测试方法
1. 创建包含 1000 条 URL 过滤规则
2. 使用 wrk 或 ab 工具进行压力测试
3. 测量 CPU 使用率和吞吐量
4. 对比 BM、KMP 和 Router 算法

### 预期性能提升
- **精确域名匹配**：30-50% 性能提升
- **子域名匹配**：20-40% 性能提升  
- **包含匹配**：0-10% 性能提升
- **内存使用**：减少 10-20%

## 稳定性测试

### 测试项目
1. **长时间运行**：连续运行 24 小时
2. **内存泄漏**：监控内核内存使用
3. **并发连接**：测试 1000+ 并发连接
4. **边界条件**：测试超长 SNI、特殊字符等

## 回归测试

确保所有原有功能正常：
- Web 管理界面 URL 过滤
- MAC 地址过滤
- 时间控制
- 忽略大小写选项

## 测试命令示例

### 基础功能测试
```bash
# 测试精确域名
curl -v https://www.google.com

# 测试子域名
curl -v https://mail.google.com

# 测试包含匹配
curl -v https://www.youtube.com
```

### 性能测试
```bash
# 安装测试工具
opkg install wrk

# 压力测试
wrk -t12 -c400 -d30s https://target-site.com
```

### 监控命令
```bash
# CPU 使用率
top

# 内存使用
free -m

# 网络统计
cat /proc/net/dev

# iptables 规则统计
iptables -L -v -n -t filter
```

## 故障排除

### 常见问题
1. **模块加载失败**：检查内核日志 dmesg
2. **规则不生效**：验证算法名称和语法
3. **性能下降**：检查是否回退到 BM 算法

### 调试命令
```bash
# 查看内核模块
lsmod | grep sni

# 查看内核日志
dmesg | grep sni

# 查看 iptables 规则
iptables -t filter -L -v -n
```

## 测试报告模板

### 性能数据
| 算法 | 规则数 | 测试时长 | CPU使用率 | 吞吐量(Mbps) |
|------|--------|----------|-----------|--------------|
| BM   | 100    | 30s      | 45%       | 850          |
| KMP  | 100    | 30s      | 42%       | 920          |
| Router| 100    | 30s      | 30%       | 1200         |

### 功能测试结果
- [ ] 精确域名匹配正常
- [ ] 子域名匹配正常
- [ ] 包含匹配正常
- [ ] 大小写不敏感正常
- [ ] Web界面配置正常
- [ ] 长时间运行稳定

## 下一步优化计划

1. **通配符优化**：实现更复杂的通配符匹配
2. **正则表达式**：添加基础正则表达式支持
3. **缓存机制**：实现匹配结果缓存
4. **多线程优化**：并行匹配多个规则

---

**测试日期**：____
**测试人员**：____
**测试结果**：____