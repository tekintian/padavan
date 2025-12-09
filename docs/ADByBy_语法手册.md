# ADByBy 自定义过滤语法完整手册

## 概述

ADByBy 是一个强大的广告过滤工具，支持基于 ABP (Adblock Plus) 规则的广告过滤，并在此基础上进行了字符替换等扩展功能的增强。本手册详细介绍了 ADByBy 支持的所有过滤语法和使用方法。

---

## 基础语法

### 1. 注释符 `!`

**功能**：用于添加注释和说明
**语法**：`! 注释内容`

**示例**：
```adblock
! 这是一条注释
! AdByBy 自定义规则开始
! 更新时间：2024-01-10
```

**说明**：
- 以 `!` 开头的行将被视为注释，不会执行过滤功能
- 注释可以放在规则的上方进行说明
- 注释行不影响过滤性能

---

## 匹配语法

### 2. 字符通配符 `*`

**功能**：匹配任意长度（包括0长度）的字符串
**语法**：`pattern*pattern`

**示例**：
```adblock
! 匹配包含广告的所有链接
*ad* 
*advertisement*
*banner*

! 匹配任意子目录
*/ads/*
*/advertisement/*
```

**限制**：
- 不能与正则表达式语法混用
- 适用于简单的字符匹配场景

---

### 3. 分隔符 `^`

**功能**：表示分隔符，匹配除字母、数字、下划线、连字符、点号、百分号之外的任何字符
**语法**：`pattern^pattern`

**示例**：
```adblock
! 精确匹配以广告相关的分隔
ads^
advertisement^
banner^

! 匹配URL中的路径分隔
domain.com^ads
example.com^banner
```

**说明**：
- `^` 可以匹配 `/`、`?`、`&`、`=`、`,`、`;`、`:` 等字符
- 常用于确保完整匹配域名或路径

---

## 精确匹配语法

### 4. 管线符 `|`

**功能**：表示地址的最前端或最末端
**语法**：
- `|pattern` - 匹配地址开头
- `pattern|` - 匹配地址结尾

**示例**：
```adblock
! 匹配地址开头
|http://ads.example.com/
|https://ads.server.com/

! 匹配地址结尾
/banner.jpg
/advert.png
```

**说明**：
- `|` 确保从URL的绝对开始或结束位置进行匹配
- 提高匹配的精确度，减少误杀

---

### 5. 子域通配符 `||`

**功能**：匹配主域名及其所有子域
**语法**：`||domain.com`

**示例**：
```adblock
! 匹配主域名及所有子域
||ads.example.com
||advertisement.server.com
||googleadservices.com
||doubleclick.net

! 匹配特定网站的广告资源
||youtube.com/ads/
||facebook.com/tr/
```

**说明**：
- `||` 后面跟域名，会匹配该域名及其所有子域
- 是最常用的域名匹配方式
- 可以结合路径进行更精确的匹配

---

## 排除语法

### 6. 排除标识符 `~`

**功能**：排除某些规则，防止误杀
**语法**：`@@pattern` 或 `~pattern`

**示例**：
```adblock
! 排除特定域名的过滤
@@||example.com/ads/
@@||trusted-site.com/banner

! 排除特定的文件类型
@@*.css$important
@@*.js$important

! 排除特定路径
@@/path/to/legitimate/content
```

**说明**：
- `@@` 或 `~` 用于创建白名单规则
- 当通用规则可能误杀时，使用排除规则进行修正
- 可以与其它语法组合使用

---

## 元素隐藏语法

### 7. 元素选择器 `##`

**功能**：隐藏页面中特定的HTML元素
**语法**：`domain.com##selector`

**示例**：
```adblock
! 隐藏特定ID的元素
example.com##ad-container
site.com##sidebar-ad

! 隐藏特定类的元素
youtube.com##.video-ads
facebook.com##.advertisement

! 隐藏特定标签组合
site.com##div[id*="ad"][class*="banner"]
```

**限制**：
- 元素隐藏暂不支持全局规则
- 元素隐藏暂不支持排除规则
- 仅支持CSS选择器语法

---

## ADByBy 扩展功能

### 8. 文本替换 `$s@pattern@replacement@`

**功能**：替换页面中的特定文本内容
**语法**：`$s@模式字符串@替换后的文本@`

**示例**：
```adblock
! 替换广告文本
$s@广告推荐@优质推荐@
$s@Sponsored@Featured@
$s@Advertisement@Promotion@

! 支持通配符
$s@*广告*@精品内容@
$s@推广*@活动信息@

! 支持通配符?
$s@推广?.*@精选活动@
```

**说明**：
- `$s@` 开始，`@` 结束
- 中间包含两部分：模式字符串和替换文本
- 支持通配符 `*` 和 `?`
- 可以进行敏感词过滤和内容优化

---

## 组合语法示例

### 1. 基础广告过滤

```adblock
! 基础广告域名过滤
||googleadservices.com
||doubleclick.net
||googlesyndication.com

! 路径过滤
*/ads/*
*/advertisement*
*/banner*

! 文件类型过滤
*/ad.js
*/banner.jpg
*.gif|domain=ads.com
```

### 2. 高级精确匹配

```adblock
! 精确域名匹配
||ads.example.com^
||tracker.server.com^

! 排除特定内容
@@||example.com/ads/non-intrusive/
@@||trusted-site.com/analytics.js

! 复合匹配
|https://ads.*.com/tracking|
*/ad/*^third-party
```

### 3. 元素隐藏

```adblock
! 隐藏广告容器
youtube.com##.ytp-ad-overlay-container
facebook.com##[data-advertiser]

! 隐藏弹窗广告
*.com##.popup-ad
*.com##.modal-advertisement
```

### 4. 文本替换

```adblock
! 替换广告文本
$s@推广@精选@
$s@Sponsored@Featured@
$s@广告位@推荐位@

! 通配符替换
$s@*推广*@精选活动@
$s@ad*@活动@
```

---

## 实用规则模板

### 1. 通用广告过滤模板

```adblock
! === 通用广告域名过滤 ===
||googleads.g.doubleclick.net
||googlesyndication.com
||googleadservices.com
||doubleclick.net

! === 广告路径过滤 ===
*/ad/*
*/ads/*
*/advertisement*
*/banner*
*/sponsor*

! === 广告文件类型 ===
*/ad.js
*/ad.jpg
*/ad.gif
*/banner.*
```

### 2. 网站定制模板

```adblock
! === YouTube 广告过滤 ===
||youtube.com/ads/
||youtube.com/get_video_info?adformat=*
youtube.com##.ytp-ad-overlay-container

! === Facebook 广告过滤 ===
||facebook.com/tr/
||facebook.com/ads/
facebook.com##[data-advertiser]
facebook.com##.ego_unit

! === 排除规则 ===
@@||youtube.com/watch?v=*
@@||facebook.com/permalink.php*
```

### 3. 内容优化模板

```adblock
! === 文本替换规则 ===
$s@广告位@推荐位@
$s@推广信息@活动信息@
$s@Sponsored@Featured@
$s@Advertisement@Promotion@

! === 通配符替换 ===
$s@*推广*@精选活动@
$s@*广告*@优质内容@
```

---

## 最佳实践

### 1. 规则编写原则

- **精确性**：尽量使用精确匹配，避免过度过滤
- **性能**：简单规则优先，复杂规则会影响性能
- **维护性**：添加清晰的注释，便于后续维护
- **测试**：定期检查规则是否有效，及时更新

### 2. 规则优化建议

```adblock
! ❌ 不推荐的写法（过于宽泛）
*ad*

! ✅ 推荐的写法（更精确）
||ads.example.com^
*/advertisement*
```

### 3. 调试方法

1. **逐步添加**：一次添加几条规则，测试效果
2. **使用排除**：发现误杀时及时添加排除规则
3. **定期检查**：定期查看日志，优化规则集

---

## 规则管理

### 1. 规则文件位置
- 主规则文件：`/etc/storage/adbyby_rules.sh`
- 黑名单：`/etc/storage/adbyby_blackip.sh`
- 白名单：`/etc/storage/adbyby_adesc.sh`
- 主机列表：`/etc/storage/adbyby_adhost.sh`

### 2. 规则更新方法

```bash
# 手动更新规则
/usr/share/adbyby/adbyby.sh A

# 重启服务
/usr/share/adbyby/adbyby.sh restart

# 查看调试信息
/usr/share/adbyby/adbyby.sh debug
```

### 3. 规则验证

```bash
# 检查规则语法
bash -n /etc/storage/adbyby_rules.sh

# 查看规则数量
grep -v '^!' /etc/storage/adbyby_rules.sh | wc -l
```

---

## 常见问题

### Q1: 规则不生效怎么办？
**A**: 检查规则语法是否正确，确保没有语法错误。重启 ADByBy 服务。

### Q2: 如何处理误杀？
**A**: 使用 `@@` 或 `~` 语法添加排除规则。

### Q3: 规则过多会影响性能吗？
**A**: 会有一定影响，建议定期清理无效规则。

### Q4: 如何测试新规则？
**A**: 使用调试模式查看匹配情况，逐步添加规则。

---

## 总结

ADByBy 的过滤语法功能强大且灵活，合理使用可以大大提升上网体验。关键要点：

1. **掌握基础语法**：`!`、`*`、`^`、`|`、`||`
2. **善用排除规则**：避免误杀重要内容
3. **利用扩展功能**：文本替换等独特功能
4. **定期维护**：保持规则集的有效性和精确性

通过本手册的指导，您可以创建出高效、精确的广告过滤规则，享受更清爽的网络体验。