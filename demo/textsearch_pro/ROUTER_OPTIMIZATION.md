# 路由器URL匹配优化指南

## 场景分析

### 路由器典型资源限制
- **内存**: 通常8MB-64MB
- **CPU**: 200MHz-1GHz（通常400MHz左右）
- **并发**: 需要同时处理多个连接
- **实时性**: 低延迟要求，不能阻塞网络流量

### URL匹配需求
- 家长控制（域名黑名单）
- 广告拦截（URL模式匹配）
- 流量分类（识别应用类型）
- 安全过滤（恶意URL检测）

## 算法选择建议

### 🥇 第一选择：固定字符串匹配（Boyer-Moore）

**适用场景**：
- 精确域名黑名单
- 已知的广告服务器域名
- 固定的URL路径模式

**优势**：
- ⚡ **性能最佳**: 平均0.178-2.704μs/搜索
- 💾 **内存最少**: 约1KB/实例
- 🔧 **实现简单**: 无通配符，预编译优化
- 📈 **可预测**: 性能稳定，不随复杂度变化

**推荐模式**：
```c
// 推荐：精确域名匹配
"www.youtube.com"
"doubleclick.net"
"google-analytics.com"

// 推荐：精确URL匹配
"/api/ad"
"/track.js"
```

### 🥈 第二选择：简单通配符（Custom Algorithm）

**适用场景**：
- 子域名匹配
- 路径前缀匹配
- 简单的URL模式

**性能优化策略**：

#### 1. 预处理优化
```c
// 避免复杂的通配符组合
// ❌ 不推荐
"https://*.sub.domain.com/path/*"

// ✅ 推荐  
"*.sub.domain.com/*"
```

#### 2. 分级匹配
```c
// 第一级：快速域名检查
if (strstr(url, "youtube.com")) {
    // 第二级：详细路径匹配
    if (strstr(url, "/watch/")) {
        block_url();
    }
}
```

#### 3. 专用算法优化
对于 `*.domain.com/*` 模式，可以实现专用算法：

```c
// 超快速的域名匹配
bool fast_domain_match(const char *url, const char *domain) {
    const char *host_start = extract_host(url);
    return (strstr(host_start, domain) != NULL);
}
```

### 🥉 第三选择：通用通配符（现有Wildcard算法）

**适用场景**：
- 复杂的URL模式匹配
- 需要完整通配符支持的场景

**限制条件**：
- 控制模式复杂度
- 限制并发实例数
- 考虑缓存机制

## 性能基准

### 内存使用分析（基于测试结果）

| 并发实例 | 简单模式(KB) | 复杂模式(KB) | 建议 |
|---------|-------------|-------------|------|
| 10      | <10         | <20         | 推荐 |
| 50      | <50         | <100        | 适中 |
| 100     | <100        | <200        | 谨慎 |
| 500     | <500        | <1000       | 不推荐 |

### CPU负载分析（400MHz CPU）

| 模式复杂度 | 平均时间(μs) | CPU负载(%) | 吞吐量(req/s) | 建议 |
|-----------|-------------|-----------|--------------|------|
| 简单域名   | 0.5-1.0     | 0.2-0.4   | 1M-2M       | 优秀 |
| 简单路径   | 1.0-2.0     | 0.4-0.8   | 500K-1M     | 良好 |
| 复杂通配符 | 3.0-5.0     | 1.2-2.0   | 200K-333K   | 一般 |
| 超复杂模式 | >5.0        | >2.0      | <200K       | 避免 |

## 实施建议

### 1. 混合策略
```c
// 分层过滤系统
typedef enum {
    FILTER_EXACT_DOMAIN,    // Boyer-Moore: 最快
    FILTER_SIMPLE_WILDCARD, // 优化算法: 较快  
    FILTER_COMPLEX_PATTERN  // 通用算法: 最慢
} filter_type_t;

// 根据URL复杂度选择算法
filter_type_t select_algorithm(const char *pattern) {
    if (strchr(pattern, '*') == NULL) {
        return FILTER_EXACT_DOMAIN;
    } else if (is_simple_wildcard(pattern)) {
        return FILTER_SIMPLE_WILDCARD;
    } else {
        return FILTER_COMPLEX_PATTERN;
    }
}
```

### 2. 缓存机制
```c
// URL匹配结果缓存
typedef struct {
    char url_hash[16];
    bool blocked;
    time_t timestamp;
} url_cache_entry_t;

// LRU缓存，最多1000条目
url_cache_entry_t url_cache[1000];
```

### 3. 内存池管理
```c
// 预分配内存，避免运行时分配
typedef struct {
    ts_config *configs[MAX_FILTERS];
    ts_state states[MAX_CONCURRENT];
    bool used[MAX_FILTERS];
} filter_pool_t;
```

### 4. 异步处理
```c
// 对于复杂匹配，使用异步处理
typedef struct {
    char url[256];
    int connection_id;
    struct filter_job *next;
} filter_job_t;

// 后台线程处理复杂匹配
void* async_filter_worker(void *arg) {
    while (true) {
        filter_job_t *job = get_filter_job();
        bool result = complex_match(job->url);
        set_filter_result(job->connection_id, result);
    }
}
```

## 具体实现方案

### 方案A：最小资源方案（<1MB内存）

**适用**：基础路由器，简单过滤需求

**实现**：
- 使用Boyer-Moore进行精确域名匹配
- 最多100个规则
- 内存使用：<100KB

```c
// 精确域名黑名单
const char *blacklist[] = {
    "malware-site.com",
    "adult-content.com",
    // ... 最多100个
};

int url_filter(const char *url) {
    char host[256];
    extract_host(url, host);
    
    for (int i = 0; i < sizeof(blacklist)/sizeof(blacklist[0]); i++) {
        if (strcmp(host, blacklist[i]) == 0) {
            return BLOCK;
        }
    }
    return ALLOW;
}
```

### 方案B：平衡方案（1-5MB内存）

**适用**：中端路由器，家长控制

**实现**：
- 混合使用Boyer-Moore和简单通配符
- 最多500个规则
- 包含缓存机制

```c
// 混合过滤规则
typedef struct {
    const char *pattern;
    filter_type_t type;
    action_t action;
} filter_rule_t;

filter_rule_t rules[] = {
    {"*.youtube.com", FILTER_SIMPLE_WILDCARD, BLOCK},
    {"*.facebook.com", FILTER_SIMPLE_WILDCARD, BLOCK},
    {"google-analytics.com", FILTER_EXACT_DOMAIN, ALLOW},
    // ... 更多规则
};
```

### 方案C：全功能方案（5-20MB内存）

**适用**：高端路由器，企业级过滤

**实现**：
- 支持所有通配符模式
- 最多2000个规则
- 包含完整的缓存和异步处理

## 性能监控指标

### 关键指标
1. **平均匹配时间** < 2μs
2. **内存使用** < 总内存的10%
3. **CPU占用** < 5%
4. **缓存命中率** > 80%

### 监控实现
```c
typedef struct {
    uint64_t total_requests;
    uint64_t cache_hits;
    uint64_t avg_match_time_us;
    size_t memory_usage;
    double cpu_usage_percent;
} performance_stats_t;

// 定期报告统计信息
void report_performance() {
    printf("Filter Performance:\n");
    printf("  Total requests: %lu\n", stats.total_requests);
    printf("  Cache hit rate: %.2f%%\n", 
           (double)stats.cache_hits / stats.total_requests * 100);
    printf("  Avg match time: %.2f μs\n", stats.avg_match_time_us);
}
```

## 总结

对于路由器这种资源受限的URL匹配场景：

1. **优先选择Boyer-Moore**进行精确域名匹配
2. **谨慎使用通配符**，限制模式复杂度
3. **实施分层过滤**，按需选择算法
4. **合理使用缓存**，减少重复计算
5. **控制并发实例**，避免内存溢出
6. **监控关键指标**，及时优化

通过这些优化措施，即使在400MHz CPU、8MB内存的路由器上，也能实现高效、稳定的URL匹配功能。