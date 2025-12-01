#!/bin/sh

echo "=== 测试SNI模块修复效果 ==="
echo "这个脚本用于验证SNI模块修复是否有效"
echo

echo "1. 检查当前内核版本和SNI模块状态："
echo "内核版本: $(uname -r)"
echo "SNI符号检查:"
cat /proc/kallsyms | grep xt_sni || echo "SNI符号未找到"
echo

echo "2. 清空现有规则并创建测试规则："
iptables -F urllist

# 创建SNI测试规则
echo "创建SNI测试规则..."
iptables -A urllist -p tcp --dport 443 -m sni --sni "news.qq.com" -j LOG --log-prefix "SNI_FIX_TEST: "
iptables -A urllist -p tcp --dport 443 -m sni --sni "news.qq.com" -j REJECT --reject-with tcp-reset

echo "3. 显示创建的规则："
iptables -L urllist -v -n --line-numbers
echo

echo "4. 清空内核日志缓冲区："
dmesg -c > /dev/null
echo "✓ 日志缓冲区已清空"
echo

echo "5. 生成测试流量："
echo "测试HTTPS连接到news.qq.com..."
timeout 10 wget -qO- https://news.qq.com >/dev/null 2>&1 &
WGET_PID=$!
sleep 3

if kill -0 $WGET_PID 2>/dev/null; then
    kill $WGET_PID 2>/dev/null
fi

echo "✓ 测试完成"
echo

echo "6. 检查规则计数器："
iptables -L urllist -v -n
echo

echo "7. 检查SNI匹配日志："
SNI_LOGS=$(dmesg | grep "SNI_FIX_TEST:")
if [ -n "$SNI_LOGS" ]; then
    echo "✓ 找到SNI匹配日志:"
    echo "$SNI_LOGS"
else
    echo "✗ 没有找到SNI匹配日志"
fi

echo
echo "8. 检查调试信息："
DEBUG_LOGS=$(dmesg | grep -E "\[SNI-FILTER\]")
if [ -n "$DEBUG_LOGS" ]; then
    echo "✓ 找到SNI调试信息:"
    echo "$DEBUG_LOGS" | tail -10
else
    echo "✗ 没有找到SNI调试信息"
fi

echo
echo "=== 测试结果分析 ==="
echo "如果看到SNI匹配日志和调试信息，说明修复成功"
echo "如果仍然没有匹配，可能需要进一步调试"
echo

echo "9. 对比测试 - webstr模块（应该工作）："
iptables -F urllist
iptables -A urllist -p tcp --dport 80 -m webstr --host "news.qq.com" -j LOG --log-prefix "WEBSTR_CONTROL: "

dmesg -c > /dev/null
timeout 5 wget -qO- http://news.qq.com >/dev/null 2>&1 &
sleep 2

WEBSTR_LOGS=$(dmesg | grep "WEBSTR_CONTROL:")
if [ -n "$WEBSTR_LOGS" ]; then
    echo "✓ webstr模块正常工作 (对照组)"
else
    echo "✗ webstr模块也有问题"
fi

echo
echo "测试完成！请检查上述输出判断修复效果。"