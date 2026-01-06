# 路由器固件时区与防火墙时间匹配问题分析

## 问题概述

**症状**: 当前路由器设置为 UTC+8 (北京时间)，防火墙规则中的时间条件与实际时间相差了 -8 小时。

**影响**: 网络访问控制功能中的时间条件无法在正确的时间生效。

---

## 根本原因

### 1. 时区表示方法的差异

在 Linux 系统中，时区信息有两种不同的表示方式：

#### 1.1 `tm_gmtoff` (struct tm 成员)
- **定义**: `long tm_gmtoff`
- **含义**: 本地时间距离 UTC 的偏移秒数（东偏为正，西偏为负）
- **示例**:
  - UTC+8 (北京时间): `tm_gmtoff = 28800` (正数)
  - UTC-5 (美国东部): `tm_gmtoff = -18000` (负数)

#### 1.2 `tz_minuteswest` (struct timezone 成员)
- **定义**: `int tz_minuteswest`
- **含义**: 距离 UTC 的西偏分钟数（西偏为正，东偏为负）
- **示例**:
  - UTC+8 (北京时间): `tz_minuteswest = -480` (负数)
  - UTC-5 (美国东部): `tz_minuteswest = 300` (正数)

**关键差异**: 两个值的符号是相反的！

---

## 代码分析

### 2. 用户空间代码 (`trunk/user/rc/src/rc.c`)

#### 2.1 `setkernel_tz()` 函数 (行 572-616)

```c
void setkernel_tz(void)
{
    time_t now;
    struct tm local, gm;
    struct timezone tz;
    struct timeval *tvp = NULL;
    static int tz_minuteswest_last = -1;
    long gmtoff = 0;

    /* Update kernel timezone */
    time(&now);
    localtime_r(&now, &local);
    gmtime_r(&now, &gm);

    /* Calculate gmtoff: seconds east of UTC (negative = west, positive = east) */
#if defined(__GLIBC__) || defined(__UCLIBC__)
    gmtoff = local.tm_gmtoff;
#else
    /* For systems without tm_gmtoff, calculate it */
    gmtoff = (long)(mktime(&local) - mktime(&gm));
#endif

    /* Convert to minutes west of UTC */
    /* tm_gmtoff: positive=east of UTC, negative=west of UTC */
    /* tz.tz_minuteswest: minutes west of UTC */
    /* For CST (UTC+8): gmtoff=28800, tz_minuteswest should be -480 */
    tz.tz_minuteswest = gmtoff / 60;  // ❌ 错误！缺少负号

    if (tz_minuteswest_last == tz.tz_minuteswest)
        return;

    tz_minuteswest_last = tz.tz_minuteswest;

    /* Debug: log timezone information */
    dbg("setkernel_tz: gmtoff=%ld, tz_minuteswest=%d, tz_name=%s, gm.tm_hour=%d, gm.tm_min=%d, local.tm_hour=%d, local.tm_min=%d",
        gmtoff, tz_minuteswest, nvram_safe_get("time_zone_x"),
        gm.tm_hour, gm.tm_min, local.tm_hour, local.tm_min);

    /* Verify calculation with a test */
    if (abs(gmtoff) % 3600 != 0) {
        dbg("setkernel_tz: WARNING - gmtoff %% 3600 = %ld, should be multiple of 3600", gmtoff);
    }

    settimeofday(tvp, &tz);
}
```

**问题所在 (行 598)**:
```c
tz.tz_minuteswest = gmtoff / 60;  // ❌ 错误
```

对于 UTC+8 (北京时间):
- `gmtoff = 28800` (正数，表示东8区)
- `tz_minuteswest = 28800 / 60 = 480` (错误的正数)

**正确应该是**:
```c
tz.tz_minuteswest = -(gmtoff / 60);  // ✓ 正确
```

对于 UTC+8 (北京时间):
- `gmtoff = 28800` (正数)
- `tz_minuteswest = -(28800 / 60) = -480` (正确的负数)

#### 2.2 `setenv_tz()` 函数 (行 562-569)

```c
void setenv_tz(void)
{
    static char TZ_env[64];

    snprintf(TZ_env, sizeof(TZ_env), "TZ=%s", nvram_safe_get("time_zone_x"));
    TZ_env[sizeof(TZ_env)-1] = '\0';
    putenv(TZ_env);
}
```

这个函数设置用户空间的时区环境变量，使用 `time_zone_x` 的值。

#### 2.3 `set_timezone()` 函数 (行 220-225)

```c
static void
set_timezone(void)
{
    time_zone_x_mapping();
    setenv_tz();
    setkernel_tz();
}
```

这是时区设置的主函数，依次调用:
1. `time_zone_x_mapping()` - 时区名称映射
2. `setenv_tz()` - 设置用户空间时区
3. `setkernel_tz()` - 设置内核时区

---

### 3. 内核空间代码 (`trunk/linux-4.4.x/kernel/time/time.c`)

#### 3.1 `do_sys_settimeofday()` 函数 (行 164-192)

```c
int do_sys_settimeofday(const struct timespec *tv, const struct timezone *tz)
{
    static int firsttime = 1;
    int error = 0;

    if (tv && !timespec_valid(tv))
        return -EINVAL;

    error = security_settime(tv, tz);
    if (error)
        return error;

    if (tz) {
        /* Verify we're within +-15 hrs range */
        if (tz->tz_minuteswest > 15*60 || tz->tz_minuteswest < -15*60)
            return -EINVAL;

        sys_tz = *tz;  // 直接复制用户空间的 tz 到内核
        update_vsyscall_tz();
        if (firsttime) {
            firsttime = 0;
            if (!tv)
                warp_clock();
        }
    }
    if (tv)
        return do_settimeofday(tv);
    return 0;
}
```

这个函数通过 `settimeofday()` 系统调用从用户空间接收时区信息，并保存到全局变量 `sys_tz`。

---

### 4. iptables time match 模块 (`trunk/linux-4.4.x/net/netfilter/xt_time.c`)

#### 4.1 `time_mt()` 函数 (行 154-230)

```c
static bool
time_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
    const struct xt_time_info *info = par->matchinfo;
    unsigned int packet_time;
    struct xtm current_time;
    s64 stamp;

    /* Get packet timestamp */
    if (skb->tstamp.tv64 == 0)
        __net_timestamp((struct sk_buff *)skb);

    stamp = ktime_to_ns(skb->tstamp);
    stamp = div_s64(stamp, NSEC_PER_SEC);

    if (info->flags & XT_TIME_LOCAL_TZ)
        /* Adjust for local timezone */
        stamp -= 60 * sys_tz.tz_minuteswest;  // 关键行！

    /* Match date range */
    if (stamp < info->date_start || stamp > info->date_stop)
        return false;

    /* Match daytime range */
    packet_time = localtime_1(&current_time, stamp);

    if (info->daytime_start < info->daytime_stop) {
        if (packet_time < info->daytime_start ||
            packet_time > info->daytime_stop)
            return false;
    } else {
        if (packet_time < info->daytime_start &&
            packet_time > info->daytime_stop)
            return false;

        if ((info->flags & XT_TIME_CONTIGUOUS) &&
             packet_time <= info->daytime_stop)
            stamp -= SECONDS_PER_DAY;
    }

    /* Match weekday */
    localtime_2(&current_time, stamp);

    if (!(info->weekdays_match & (1 << current_time.weekday)))
        return false;

    /* Match monthday */
    if (info->monthdays_match != XT_TIME_ALL_MONTHDAYS) {
        localtime_3(&current_time, stamp);
        if (!(info->monthdays_match & (1 << current_time.monthday)))
            return false;
    }

    return true;
}
```

**关键行 (行 179)**:
```c
stamp -= 60 * sys_tz.tz_minuteswest;
```

这是将 UTC 时间戳转换为本地时间的关键计算。

---

## 错误传播路径

```
1. 用户选择时区: UTC+8 (CST-8)
   ↓
2. time_zone_x_mapping()
   设置 time_zone_x = "CST-8"
   ↓
3. setenv_tz()
   设置 TZ 环境变量
   ↓
4. setkernel_tz()
   计算出 gmtoff = 28800 (正数)
   错误地设置 tz_minuteswest = 480 (应该是 -480)
   ↓
5. settimeofday(tvp, &tz)
   将错误的 tz_minuteswest 传递给内核
   ↓
6. 内核 sys_tz.tz_minuteswest = 480
   ↓
7. xt_time 模块匹配时:
   stamp -= 60 * 480
   stamp -= 28800  (减去 8 小时!)
   ↓
8. 结果: UTC 时间被错误地转换为 UTC-8
   防火墙规则晚 8 小时生效
```

---

## 修复方案

### 修改 `trunk/user/rc/src/rc.c` 第 598 行

**修改前**:
```c
/* Convert to minutes west of UTC */
/* tm_gmtoff: positive=east of UTC, negative=west of UTC */
/* tz.tz_minuteswest: minutes west of UTC */
/* For CST (UTC+8): gmtoff=28800, tz_minuteswest should be -480 */
tz.tz_minuteswest = gmtoff / 60;
```

**修改后**:
```c
/* Convert to minutes west of UTC */
/* tm_gmtoff: positive=east of UTC, negative=west of UTC */
/* tz.tz_minuteswest: minutes west of UTC */
/* For CST (UTC+8): gmtoff=28800, tz_minuteswest should be -480 */
tz.tz_minuteswest = -(gmtoff / 60);  // 添加负号
```

### 代码注释同步更新

为了确保代码可维护性，同步更新了以下文件的注释：

#### 1. `trunk/user/rc/src/firewall_ex.c` - `timematch_conv()` 函数注释
- **位置**: 行 297-327
- **更新内容**: 增加了详细的时区处理机制说明
- **关键点**:
  - 说明 `start_firewall_ex()` 在生成规则前调用 `setkernel_tz()`
  - 说明内核时区变量 `sys_tz.tz_minuteswest` 的使用
  - 说明 `--kerneltz` 参数的作用
  - 增加了时区转换公式说明
  - 记录了修复的关键点：`tz_minuteswest = -(gmtoff / 60)`

#### 2. `trunk/user/rc/src/firewall_ex.c` - `start_firewall_ex()` 函数注释
- **位置**: 行 2933-2974
- **更新内容**: 扩展了时区设置流程说明
- **关键点**:
  - 详细说明了 `time_zone_x_mapping()` 和 `setkernel_tz()` 的作用
  - 记录了修复内容：解决符号问题
  - 添加了计算示例
  - 说明了内核时间戳转换的实际效果

#### 3. `trunk/user/rc/src/firewall_ex.c` - 代码内注释
- **位置**: 行 357-364 (在 `timematch_conv()` 函数内)
- **更新内容**: 添加了详细的时区处理流程注释
- **关键点**:
  - 说明时区设置的调用链
  - 解释 `tm_gmtoff` 和 `tz_minuteswest` 的转换关系
  - 展示内核时间戳转换的实际计算过程

---

## 验证测试

### 修改前 (错误行为)
```
设置时区: UTC+8 (CST-8)
gmtoff = 28800
tz_minuteswest = 480  (错误)

内核计算:
stamp (UTC) = 10000000
stamp -= 60 * 480
stamp = 10000000 - 28800 = 9971200

结果: 时间戳减少了 8 小时
实际效果: UTC 时间变成 UTC-8
防火墙规则: 晚 8 小时生效 ❌
```

### 修改后 (正确行为)
```
设置时区: UTC+8 (CST-8)
gmtoff = 28800
tz_minuteswest = -480  (正确)

内核计算:
stamp (UTC) = 10000000
stamp -= 60 * (-480)
stamp = 10000000 + 28800 = 10028800

结果: 时间戳增加了 8 小时
实际效果: UTC 时间变成 UTC+8 (北京时间)
防火墙规则: 在正确时间生效 ✓
```

---

## 时区数据流转总结

### 用户空间时区设置
1. **Web 界面设置**: `time_zone = "PRC"` 或其他时区
2. **映射转换**: `time_zone_x_mapping()` → `time_zone_x = "CST-8"`
3. **用户空间 TZ**: `setenv_tz()` → `TZ=CST-8` 环境变量
4. **内核时区**: `setkernel_tz()` → `sys_tz.tz_minuteswest`

### 防火墙规则生成
1. **规则配置**: `timematch_conv()` 生成 iptables time match 规则
2. **使用 --kerneltz**: 规则使用 `--kerneltz` 参数
3. **内核匹配**: `xt_time` 模块使用 `sys_tz.tz_minuteswest` 进行时间匹配

---

## 相关文件清单

| 文件路径 | 作用 |
|---------|------|
| `trunk/user/rc/src/rc.c` | 系统初始化、时区设置主逻辑 |
| `trunk/user/shared/src/shutils.c` | `time_zone_x_mapping()` 时区名称映射 |
| `trunk/user/rc/src/firewall_ex.c` | 防火墙规则生成，包括时间匹配规则 |
| `trunk/user/iptables/iptables-1.8.7/extensions/libxt_time.c` | iptables time match 用户空间库 |
| `trunk/linux-4.4.x/net/netfilter/xt_time.c` | 内核 time match 模块 |
| `trunk/linux-4.4.x/kernel/time/time.c` | 内核时间系统，`do_sys_settimeofday()` |
| `trunk/user/iptables/iptables-1.8.7/include/linux/netfilter/xt_time.h` | time match 数据结构定义 |

---

## 参考资料

### POSIX 时区标准
- `TZ` 环境变量格式: `std offset dst [offset],rule`
- 示例: `CST-8` (UTC+8，无夏令时)

### struct timezone 定义
```c
struct timezone {
    int tz_minuteswest;  /* Minutes west of UTC */
    int tz_dsttime;     /* Type of DST correction */
};
```

### struct tm 定义
```c
struct tm {
    int tm_sec;     /* Seconds [0,60] */
    int tm_min;     /* Minutes [0,59] */
    int tm_hour;    /* Hour [0,23] */
    int tm_mday;    /* Day [1,31] */
    int tm_mon;     /* Month [0,11] */
    int tm_year;    /* Year - 1900 */
    int tm_wday;    /* Day of week [0,6] */
    int tm_yday;    /* Day of year [0,365] */
    int tm_isdst;   /* Daylight savings flag */
    long tm_gmtoff; /* Seconds east of UTC */
    char *tm_zone;  /* Timezone abbreviation */
};
```

---

## 总结

本次修复的核心问题在于**时区表示方法的符号转换**:

1. `tm_gmtoff`: 东偏为正 (UTC+8 = +28800)
2. `tz_minuteswest`: 西偏为正 (UTC+8 = -480)

**修复**: 在 `setkernel_tz()` 函数中添加负号，将东偏秒数正确转换为西偏分钟数:
```c
tz.tz_minuteswest = -(gmtoff / 60);
```

修改后，防火墙的时间匹配规则将在正确的时间生效，解决与实际时间相差 8 小时的问题。
