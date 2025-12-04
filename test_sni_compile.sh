#!/bin/bash
# 简单的SNI模块编译测试脚本

echo "Testing SNI filter module compilation..."

# 进入模块目录
cd /Volumes/csdisk/padavan/trunk/linux-4.4.x/net/netfilter

# 检查源码文件是否存在
if [ ! -f "xt_sni_filter.c" ]; then
    echo "Error: xt_sni_filter.c not found!"
    exit 1
fi

echo "Source file found: xt_sni_filter.c"

# 简单的语法检查（检查括号匹配、分号等）
echo "Performing basic syntax checks..."

# 检查括号是否匹配
open_braces=$(grep -o '{' xt_sni_filter.c | wc -l)
close_braces=$(grep -o '}' xt_sni_filter.c | wc -l)
echo "Open braces: $open_braces, Close braces: $close_braces"

if [ $open_braces -ne $close_braces ]; then
    echo "Warning: Brace mismatch detected!"
else
    echo "Brace matching OK"
fi

# 检查分号
semi_colons=$(grep -o ';' xt_sni_filter.c | wc -l)
echo "Semicolons found: $semi_colons"

# 检查函数定义
functions=$(grep -E '^[a-zA-Z_][a-zA-Z0-9_]*\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\(' xt_sni_filter.c | wc -l)
echo "Functions found: $functions"

# 检查是否有明显的语法错误
echo "Checking for common syntax errors..."

# 检查是否有未闭合的字符串
unclosed_strings=$(grep -o '"[^"]*$' xt_sni_filter.c | wc -l)
if [ $unclosed_strings -gt 0 ]; then
    echo "Warning: Possible unclosed string literals detected!"
else
    echo "String literals appear to be properly closed"
fi

echo "Basic syntax check completed."
echo "For full compilation, use the Docker environment:"
echo "docker run -it --name padavan -v /Volumes/csdisk/padavan/trunk:/padavan/trunk tekintian/padavan-compiler:4.4.198"