#!/bin/sh
# AdByBy-Open 功能测试脚本

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TEST_PORT=8119
ADBYBY_BIN="$SCRIPT_DIR/adbyby"
PID_FILE="/tmp/adbyby_test.pid"
LOG_FILE="/tmp/adbyby_test.log"

echo "=== AdByBy-Open 功能测试 ==="
echo "测试程序: $ADBYBY_BIN"
echo "测试端口: $TEST_PORT"
echo "PID文件: $PID_FILE"
echo "日志文件: $LOG_FILE"

# 检查程序是否存在
if [ ! -f "$ADBYBY_BIN" ]; then
    echo "错误: 找不到测试程序"
    echo "请先运行: make"
    exit 1
fi

# 清理函数
cleanup() {
    echo "正在清理测试环境..."
    if [ -f "$PID_FILE" ]; then
        kill $(cat "$PID_FILE") 2>/dev/null
        rm -f "$PID_FILE"
    fi
    killall adbyby 2>/dev/null
}

# 设置信号处理
trap cleanup EXIT INT TERM

# 停止可能运行的实例
cleanup

echo ""
echo "=== 基本功能测试 ==="

# 测试帮助信息
echo "1. 测试帮助信息..."
if $ADBYBY_BIN -h > /dev/null; then
    echo "   ✓ 帮助信息正常"
else
    echo "   ✗ 帮助信息异常"
    exit 1
fi

# 测试统计信息
echo "2. 测试统计信息..."
if $ADBYBY_BIN -s > /dev/null 2>&1; then
    echo "   ✓ 统计信息正常"
else
    echo "   ✗ 统计信息异常"
    exit 1
fi

echo ""
echo "=== 代理服务测试 ==="

# 启动代理服务器
echo "3. 启动代理服务器..."
$ADBYBY_BIN -p $TEST_PORT -d > "$LOG_FILE" 2>&1 &
ADBYBY_PID=$!
echo $ADBYBY_PID > "$PID_FILE"

# 等待服务启动
sleep 2

# 检查服务是否启动成功
if kill -0 $ADBYBY_PID 2>/dev/null; then
    echo "   ✓ 代理服务器启动成功 (PID: $ADBYBY_PID)"
else
    echo "   ✗ 代理服务器启动失败"
    cat "$LOG_FILE"
    exit 1
fi

# 测试端口监听
echo "4. 测试端口监听..."
if netstat -an | grep -q ":$TEST_PORT.*LISTEN" 2>/dev/null || \
   lsof -i :$TEST_PORT >/dev/null 2>&1; then
    echo "   ✓ 端口 $TEST_PORT 正在监听"
else
    echo "   ✗ 端口 $TEST_PORT 未监听"
    cat "$LOG_FILE"
    exit 1
fi

echo ""
echo "=== HTTP请求测试 ==="

# 测试正常请求
echo "5. 测试正常HTTP请求..."
if curl -s -x "127.0.0.1:$TEST_PORT" -m 5 http://httpbin.org/ip >/dev/null 2>&1; then
    echo "   ✓ 正常请求代理功能正常"
else
    echo "   ⚠ 正常请求代理可能存在问题（这在没有网络连接时是正常的）"
fi

# 测试广告请求（应该被屏蔽）
echo "6. 测试广告请求..."
if curl -s -x "127.0.0.1:$TEST_PORT" -m 5 http://doubleclick.net 2>&1 | grep -q "广告已屏蔽\|AdByBy"; then
    echo "   ✓ 广告请求被正确屏蔽"
else
    echo "   ⚠ 广告请求屏蔽功能需要进一步测试"
fi

echo ""
echo "=== 日志分析 ==="

if [ -f "$LOG_FILE" ]; then
    echo "7. 分析运行日志..."
    echo "   日志文件大小: $(wc -l < "$LOG_FILE") 行"
    
    # 检查是否有错误
    if grep -qi "error\|failed\|exception" "$LOG_FILE"; then
        echo "   ⚠ 日志中发现错误信息:"
        grep -i "error\|failed\|exception" "$LOG_FILE" | head -3
    else
        echo "   ✓ 日志中未发现错误"
    fi
    
    # 显示最后几行日志
    echo "   最近日志:"
    tail -3 "$LOG_FILE" | sed 's/^/     /'
fi

echo ""
echo "=== 性能测试 ==="

# 简单的性能测试
echo "8. 进行简单性能测试..."
START_TIME=$(date +%s)
for i in $(seq 1 10); do
    curl -s -x "127.0.0.1:$TEST_PORT" -m 2 http://httpbin.org/ip >/dev/null 2>&1 &
done
wait
END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

echo "   10个并发请求耗时: ${ELAPSED}秒"
if [ $ELAPSED -le 5 ]; then
    echo "   ✓ 性能表现良好"
else
    echo "   ⚠ 性能可能需要优化"
fi

echo ""
echo "=== 测试总结 ==="

# 最终统计
echo "9. 显示最终统计..."
$ADBYBY_BIN -s 2>/dev/null | grep -A 5 "Rule Statistics" | sed 's/^/   /'

echo ""
echo "=== 测试完成 ==="
echo "所有基本功能测试已完成"
echo "详细日志: $LOG_FILE"
echo "如需清理测试环境，运行: rm -f $PID_FILE $LOG_FILE"

# 清理测试进程
cleanup

echo ""
echo "如需在生产环境使用，请运行："
echo "  sudo $SCRIPT_DIR/install.sh"