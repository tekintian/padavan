#!/bin/bash

# SNI Router Performance Monitor
# 测试和监控 SNI 路由器优化的性能

SCRIPT_DIR="$(dirname "$0")"
LOG_FILE="/tmp/sni_perf_test.log"
STATS_FILE="/proc/debugfs/sni_router/stats"  # 如果debugfs可用
TEST_URLS=(
    "https://www.google.com"
    "https://mail.google.com" 
    "https://www.youtube.com"
    "https://www.facebook.com"
    "https://www.github.com"
    "https://stackoverflow.com"
    "https://www.wikipedia.org"
    "https://www.reddit.com"
    "https://www.amazon.com"
    "https://www.twitter.com"
)

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log() {
    echo -e "${GREEN}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} $1" | tee -a "$LOG_FILE"
}

warn() {
    echo -e "${YELLOW}[$(date '+%Y-%m-%d %H:%M:%S')] WARNING:${NC} $1" | tee -a "$LOG_FILE"
}

error() {
    echo -e "${RED}[$(date '+%Y-%m-%d %H:%M:%S')] ERROR:${NC} $1" | tee -a "$LOG_FILE"
}

info() {
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')] INFO:${NC} $1" | tee -a "$LOG_FILE"
}

# 检查系统状态
check_system() {
    log "检查系统状态..."
    
    # 检查 SNI 模块
    if lsmod | grep -q "xt_sni"; then
        log "✓ SNI 模块已加载"
    else
        error "✗ SNI 模块未加载"
        return 1
    fi
    
    # 检查内核日志
    if dmesg | grep -q "SNI match v3"; then
        log "✓ 优化版本的 SNI 模块已加载"
    else
        warn "⚠ 可能不是优化版本的 SNI 模块"
    fi
    
    # 检查 iptables 规则
    local rule_count=$(iptables -t filter -L | grep -c "sni" || echo "0")
    log "当前 SNI 规则数量: $rule_count"
    
    # 检查系统负载
    local load_avg=$(uptime | awk -F'load average:' '{print $2}')
    log "系统负载: $load_avg"
    
    # 检查内存使用
    local mem_usage=$(free | grep Mem | awk '{printf "%.1f%%", $3/$2 * 100.0}')
    log "内存使用: $mem_usage"
}

# 获取 SNI 统计信息
get_sni_stats() {
    if [[ -f "$STATS_FILE" ]]; then
        info "SNI 优化统计信息:"
        cat "$STATS_FILE" | while read line; do
            echo "  $line"
        done
        return 0
    fi
    
    # 尝试从内核日志获取统计
    local stats=$(dmesg | grep "SNI" | tail -10)
    if [[ -n "$stats" ]]; then
        info "最近的 SNI 统计信息:"
        echo "$stats" | while read line; do
            echo "  $line"
        done
    else
        warn "无法获取 SNI 统计信息"
    fi
}

# 性能测试函数
performance_test() {
    local iterations=${1:-10}
    local concurrent=${2:-5}
    
    log "开始性能测试 - 迭代: $iterations, 并发: $concurrent"
    
    # 创建临时结果文件
    local result_file="/tmp/sni_test_results.$$"
    > "$result_file"
    
    # 记录开始时间
    local start_time=$(date +%s.%N)
    
    # 并发测试
    for ((i=1; i<=iterations; i++)); do
        log "第 $i 次测试..."
        
        # 并发请求
        for ((j=0; j<concurrent; j++)); do
            {
                local url="${TEST_URLS[$((j % ${#TEST_URLS[@]}))]}"
                local req_start=$(date +%s.%N)
                
                # 使用 curl 进行测试 (只检查连接时间)
                local response=$(curl -s -o /dev/null -w "%{http_code},%{time_connect},%{time_total}" \
                                 --connect-timeout 5 --max-time 10 "$url" 2>/dev/null)
                
                local req_end=$(date +%s.%N)
                local req_time=$(echo "$req_end - $req_start" | bc -l 2>/dev/null || echo "0")
                
                echo "$i,$j,$url,$req_time,$response" >> "$result_file"
            } &
        done
        
        # 等待所有并发请求完成
        wait
        
        # 短暂休息
        sleep 1
    done
    
    # 记录结束时间
    local end_time=$(date +%s.%N)
    local total_time=$(echo "$end_time - $start_time" | bc -l 2>/dev/null || echo "0")
    
    # 分析结果
    log "性能测试完成，总用时: ${total_time}s"
    
    # 计算统计信息
    local total_requests=$((iterations * concurrent))
    local successful_requests=$(awk -F',' '$4 != "0" && $4 != ""' "$result_file" | wc -l)
    local avg_time=$(awk -F',' 'NR > 1 && $4 != "" { sum += $4; count++ } END { if(count > 0) print sum/count; else print 0 }' "$result_file")
    
    log "总请求数: $total_requests"
    log "成功请求: $successful_requests"
    log "成功率: $(echo "scale=2; $successful_requests * 100 / $total_requests" | bc -l)%"
    log "平均响应时间: ${avg_time}s"
    
    # 清理
    rm -f "$result_file"
    
    # 获取 SNI 统计
    get_sni_stats
}

# 负载测试
load_test() {
    local duration=${1:-60}  # 默认测试60秒
    local concurrency=${2:-10}
    
    log "开始负载测试 - 持续时间: ${duration}s, 并发数: $concurrency"
    
    # 记录开始统计
    local start_stats=$(get_sni_stats_simple)
    local start_time=$(date +%s)
    
    # 使用 wrk 进行负载测试 (如果可用)
    if command -v wrk >/dev/null 2>&1; then
        log "使用 wrk 进行负载测试"
        wrk -t"$concurrency" -c"$concurrency" -d"${duration}s" --timeout 10s \
            https://www.google.com 2>/dev/null | tee -a "$LOG_FILE"
    else
        # 使用简单的 curl 循环
        log "使用 curl 进行负载测试"
        local end_time=$((start_time + duration))
        local request_count=0
        
        while [[ $(date +%s) -lt $end_time ]]; do
            for ((i=0; i<concurrency; i++)); do
                {
                    local url="${TEST_URLS[$((i % ${#TEST_URLS[@]}))]}"
                    curl -s -o /dev/null --connect-timeout 2 --max-time 5 "$url" >/dev/null 2>&1 &
                    ((request_count++))
                }
                wait
                sleep 0.1
            done
        done
        
        log "负载测试完成，总请求数: $request_count"
    fi
    
    # 记录结束统计
    local end_stats=$(get_sni_stats_simple)
    local end_time=$(date +%s)
    
    log "负载测试用时: $((end_time - start_time))秒"
    
    # 比较统计差异
    info "测试前后统计对比:"
    info "测试前: $start_stats"
    info "测试后: $end_stats"
}

# 简化的统计获取
get_sni_stats_simple() {
    if [[ -f "$STATS_FILE" ]]; then
        grep "Total Matches" "$STATS_FILE" 2>/dev/null | awk '{print $3}' || echo "0"
    else
        echo "N/A"
    fi
}

# 内存使用监控
monitor_memory() {
    local duration=${1:-30}
    
    log "开始内存监控 - 持续时间: ${duration}s"
    
    local start_time=$(date +%s)
    local end_time=$((start_time + duration))
    
    > "/tmp/sni_memory_$$"
    
    while [[ $(date +%s) -lt $end_time ]]; do
        local timestamp=$(date '+%H:%M:%S')
        local mem_info=$(free -m | grep "Mem:")
        local sni_mem=$(cat /proc/meminfo | grep -E "(Slab|SReclaimable)" | awk '{sum += $2} END {print sum/1024}')
        
        echo "$timestamp,$mem_info,$sni_mem" >> "/tmp/sni_memory_$$"
        sleep 2
    done
    
    # 分析内存使用
    log "内存监控完成"
    info "峰值内存使用:"
    awk -F',' '{print $1 " Slab: " $3 "MB"}' "/tmp/sni_memory_$$" | sort -k3 -n | tail -5
    
    rm -f "/tmp/sni_memory_$$"
}

# 主函数
main() {
    local test_type=${1:-"all"}
    
    log "=== SNI 路由器性能监控 ==="
    log "测试类型: $test_type"
    
    case "$test_type" in
        "check")
            check_system
            get_sni_stats
            ;;
        "perf"|"performance")
            check_system
            performance_test "${2:-10}" "${3:-5}"
            ;;
        "load")
            check_system
            load_test "${2:-60}" "${3:-10}"
            ;;
        "memory")
            monitor_memory "${2:-30}"
            ;;
        "stats")
            get_sni_stats
            ;;
        "all")
            check_system
            echo ""
            performance_test 5 3
            echo ""
            monitor_memory 10
            echo ""
            get_sni_stats
            ;;
        *)
            echo "用法: $0 [check|perf|load|memory|stats|all] [参数...]"
            echo ""
            echo "选项:"
            echo "  check                    - 检查系统状态"
            echo "  perf [迭代数] [并发数]   - 性能测试"
            echo "  load [秒数] [并发数]     - 负载测试"
            echo "  memory [秒数]            - 内存监控"
            echo "  stats                   - 显示统计信息"
            echo "  all                     - 执行所有测试"
            echo ""
            echo "示例:"
            echo "  $0 check              # 只检查系统状态"
            echo "  $0 perf 20 5          # 20次迭代，5个并发"
            echo "  $0 load 120 15        # 120秒负载测试，15个并发"
            exit 1
            ;;
    esac
    
    log "测试完成，日志保存在: $LOG_FILE"
}

# 检查依赖
if ! command -v curl >/dev/null 2>&1; then
    error "curl 未安装，请先安装 curl"
    exit 1
fi

if ! command -v bc >/dev/null 2>&1; then
    warn "bc 未安装，某些计算可能不准确"
fi

# 执行主函数
main "$@"