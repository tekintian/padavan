#!/bin/bash
# 测试adbyby代理功能

echo "=== AdByBy代理功能测试 ==="

# 检查adbyby进程
echo "1. 检查adbyby进程状态:"
ps | grep adbyby | grep -v grep

# 检查端口监听
echo -e "\n2. 检查端口8118监听状态:"
netstat -ln | grep 8118

# 测试状态页面
echo -e "\n3. 测试状态页面:"
curl -s --connect-timeout 5 http://127.0.0.1:8118/ | head -n 10

# 测试HTTP转发功能（非广告站点）
echo -e "\n4. 测试HTTP转发功能:"
curl -s --connect-timeout 5 -I http://httpbin.org/ip | head -n 5

# 测试广告拦截功能
echo -e "\n5. 测试广告拦截功能:"
curl -s --connect-timeout 5 -I http://doubleclick.net | head -n 5

echo -e "\n=== 测试完成 ==="