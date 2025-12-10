


url过滤功能通过在Advanced_URLFilter_Content.asp页面设置 过滤规则:
启用网址过滤?	已启用 未启用

启用日期:	
启用时间:	
过滤主机 MAC 地址:	 Single MAC /  MAC Group  (Using MAC Address Filter groups )
url 过滤只在mac条件为MAC Group时才使用mac过滤里面的mac地址作为mac限制条件, 如果mac 过滤未启用或者mac过滤的mac为空,则url里面的规则适用全部设备(既不加mac限制)

排除   这个是是否是排除条件
网址过滤表:
qq.com 精确过滤
*qq.com  包含过滤
*.qq.com 子域名过滤
http://www.douyin.com  指定过滤特点的http协议


未指定http协议的规则,则自动过滤所有tcp协议(http  https)

ip地址过滤规则 支持过滤单个IP或者IP子网, 如  123.2.2.3  或者 123.2.2.0/24


Single MAC模式时只要提供了mac地址就必须要加上mac限制, MAC Group 模式如果mac过滤启用且mac过滤里面有mac加限制,没有或者mac过滤未启用不加mac限制,  一句话就: 不管那种模式有mac地址就加限制,没有就不加




## 支持的网址过滤表示例
在添加过滤规则时,如果没有指定http协议,则默认全部协议, 如果指定了http协议,只仅匹配指定的协议.
另外如果规则中包含了uri路径或者端口,则直接使用 textsearch来进行精确匹配, 
如 abc.com/ads   https://abc.com/ads   https://abc.com:8080/ads  或者是 htts://123.com:8080



### IP地址格式
ip地址类型的直接生成iptable的IP出站过滤规则

```
192.168.1.100           # 单个IP
192.168.1.0/24          # CIDR网段
10.0.0.0/8              # 大网段
https://172.16.0.5      # 带URL前缀
http://10.0.0.0/16      # HTTP前缀+CIDR
```

### 域名格式
```
google.com              # 精确匹配
*.youtube.com           # 子域名匹配
*facebook              # 包含匹配
https://*facebook.com  # 特定https协议的包含匹配
http://abc.com/ads # 直接使用http匹配 不使用sni
https://*.abc.com/ads # 这个就直接走SNI的 *.abc.com 匹配

```

底层技术实现: 2种方式来匹配, 1种是带sni语义的匹配  和 textsearch匹配 
在用户空间根据规则自动选择后生成对应的匹配规则,
在内核空间尽量精简高效处理相应的规则.

注意: 将所有可以在用户空间计算的逻在用户空间完成计算, 不要在内核空间计算,避免影响用户上网体验


// 加载xt_sni模块条件：防火墙和URL匹配功能都开启
   if (nvram_match("fw_enable_x", "1") && nvram_match("url_enable_x", "1")) {
      system("modprobe xt_sni");
   }


iptables -A FORWARD -p tcp --dport 443 -m sni --str "douyin.com" -j REJECT --reject-with tcp-reset

iptables -A FORWARD -p tcp -m sni --str "news.qq.com" -j DROP
iptables -A urllist -p tcp --dport 443 -m sni --str "douyin.com" -j REJECT --reject-with tcp-reset


iptables -A FORWARD -p tcp --dport 443 -m sni --str "douyin.com" -j REJECT --reject-with tcp-reset

iptables -A FORWARD -p tcp -m sni --str "news.qq.com" -j DROP


iptables -A FORWARD -p tcp -m string --string "douyin.com" --algo bm -j REJECT --reject-with tcp-reset


dstip = ip_conv("vts_ipaddr_x", i);
      if (!is_valid_ipv4(dstip))
         continue;



# URL过滤功能需求文档（优化版）

## 热门短视频平台拦截功能更新

### 功能描述
原"拦截抖音 APP 和网站"功能已升级为"拦截热门短视频平台 APP和网站"，支持拦截包括抖音、快手、微信视频、小红书、QQ短视频等在内的主流短视频平台。

### 支持的短视频平台
1. **抖音/TikTok系**：抖音、TikTok、西瓜视频、火山小视频等字节跳动系产品
2. **快手系**：快手、快手极速版等
3. **腾讯系**：微信视频号、QQ短视频、微视等
4. **小红书系**：小红书、RED等
5. **其他平台**：美拍、梨视频等

### 技术实现
- 配置变量：`block_shortvideo`（原`block_douyin`）
- 规则文件：`/etc/storage/dnsmasq-adbyby.d/08-dnsmasq.shortvideo`
- 支持DNS域名拦截，覆盖各平台的主域名、CDN域名、API域名等

### 用户界面
Web管理页面中的选项已更新为"拦截热门短视频平台 APP和网站"，用户可一键开启对所有支持短视频平台的拦截。
## 一、功能概述
URL过滤功能通过`Advanced_URLFilter_Content.asp`页面配置规则，基于**内核态iptables模块**实现高效过滤，兼顾灵活性与路由器资源限制。核心支持域名/IP多格式过滤、MAC/全局模式切换、协议/路径精准匹配，所有复杂逻辑在用户空间完成计算，内核仅执行精简规则，确保上网体验无感知。

## 二、核心配置项
| 配置项                | 说明                                                                 |
|-----------------------|----------------------------------------------------------------------|
| 启用网址过滤          | 总开关，启用后规则下发至内核态生效；未启用则清空所有过滤规则          |
| 启用日期              | 可选周日至周六，规则仅在选中日期生效（内核time模块适配）             |
| 启用时间              | 格式`HH:MM`，支持时间段（如08:00-18:00），规则仅在该时间段生效       |
| 过滤主机MAC地址模式   | Single MAC / MAC Group（复用MAC过滤组配置）                          |
| 排除条件              | 反向规则：勾选后规则从“拒绝访问”变为“允许访问”                       |
| 网址/IP过滤表         | 核心规则配置区，支持域名、IP、协议、路径、端口等多维度匹配           |

## 三、MAC限制规则（核心逻辑）
1. **Single MAC模式**：
   - 若填写MAC地址 → 内核规则添加`-m mac --mac-source`限制，仅对该MAC生效；
   - 若未填写MAC地址 → 无MAC限制，规则对所有设备生效。
2. **MAC Group模式**：
   - 若MAC过滤功能启用且过滤组有MAC地址 → 内核规则添加MAC组限制；
   - 若MAC过滤未启用/过滤组无MAC → 无MAC限制，规则对所有设备生效。
3. **通用原则**：无论哪种模式，**有有效MAC地址则加限制，无则不加**。

## 四、过滤规则格式规范
### 4.1 域名/URL格式（自动适配匹配方式）
| 规则示例                  | 匹配类型       | 协议范围       | 内核匹配方式                | 说明                                   |
|---------------------------|----------------|----------------|-----------------------------|----------------------------------------|
| google.com                | 精确匹配       | 所有协议（TCP）| SNI语义匹配 + string模块    | 匹配所有协议下的google.com域名         |
| *.youtube.com             | 子域名匹配     | 所有协议（TCP）| SNI语义匹配（*.youtube.com）| 匹配youtube.com所有子域名              |
| *facebook                 | 包含匹配       | 所有协议（TCP）| string模块（包含facebook）  | 匹配任意包含facebook的URL/域名         |
| https://*facebook.com     | 包含匹配       | 仅HTTPS        | SNI+string（HTTPS+facebook） | 仅匹配HTTPS协议下包含facebook.com的请求|
| http://abc.com/ads        | 路径精确匹配   | 仅HTTP         | textsearch（完整路径）      | 不使用SNI，直接匹配HTTP请求的完整路径  |
| https://abc.com:8080/ads  | 端口+路径匹配  | 仅HTTPS（8080）| textsearch（完整URL）       | 匹配指定端口+路径的HTTPS请求           |
| https://*.abc.com/ads     | 子域名+路径    | 仅HTTPS        | SNI（*.abc.com）+路径匹配   | SNI匹配子域名，textsearch匹配路径      |

### 4.2 IP地址格式（独立IP出站过滤）
| 规则示例                  | 匹配类型       | 协议范围       | 内核匹配方式                | 说明                                   |
|---------------------------|----------------|----------------|-----------------------------|----------------------------------------|
| 192.168.1.100             | 单个IP         | 所有协议       | `-d 192.168.1.100`          | 过滤目标IP为192.168.1.100的所有请求    |
| 192.168.1.0/24            | CIDR网段       | 所有协议       | `-d 192.168.1.0/24`         | 过滤目标网段为192.168.1.0/24的请求     |
| 10.0.0.0/8                | 大网段         | 所有协议       | `-d 10.0.0.0/8`             | 过滤目标网段为10.0.0.0/8的请求         |
| https://172.16.0.5        | 单个IP+HTTPS   | 仅HTTPS        | `-d 172.16.0.5 + HTTPS匹配`  | 仅过滤HTTPS协议下访问172.16.0.5的请求  |
| http://10.0.0.0/16        | CIDR+HTTP      | 仅HTTP         | `-d 10.0.0.0/16 + HTTP匹配`  | 仅过滤HTTP协议下访问10.0.0.0/16的请求  |

### 4.3 协议匹配规则
- 未指定协议（如`abc.com`）→ 默认匹配所有TCP协议（HTTP/HTTPS等）；
- 指定协议（如`http://abc.com`）→ 仅匹配该协议的请求；
- 包含端口（如`abc.com:8080`）→ 内核添加`--dport 8080`限制，仅匹配指定端口。

## 五、技术实现规范（用户空间+内核空间分离）
### 5.1 用户空间处理逻辑（核心优化）
1. **规则解析**：
   - 识别规则类型（域名/IP/协议/路径/端口）；
   - 转换时间格式（`HH:MM`→`HH:MM:SS`，适配内核time模块）；
   - 转换日期格式（周日=0→内核周日=7）；
   - 拆分MAC组为单个MAC地址列表；
   - 自动选择匹配算法（SNI语义/string/bm算法）。
2. **规则精简**：
   - 合并重复规则，避免内核冗余；
   - 限制单规则长度（关键词≥2字符），减少内核计算量；
   - 最大规则数≤16条，防止内核表溢出。
3. **规则序列化**：
   - 采用极简字符串格式（`mac|date|time|inv|rule`），无JSON解析开销；
   - 下发前完成所有计算，内核仅执行最终iptables命令。

### 5.2 内核空间执行逻辑（极致精简）
1. **匹配算法**：
   - 域名/URL匹配：使用`--algo bm`（Boyer-Moore）高效字符串匹配；
   - 范围限制：`--to 65535`仅匹配TCP载荷前64KB，覆盖URL/域名核心区域。
2. **模块复用**：
   - MAC限制：`-m mac --mac-source`；
   - 时间限制：`-m time --weekdays --timestart --timestop`；
   - IP限制：`-d [IP/CIDR]`；
   - 协议/端口：`-p tcp --dport`；
   - 字符串匹配：`-m string --string --algo bm`。
3. **动作执行**：
   - 正常规则：`-j REJECT`（内核态直接拒绝，无延迟）；
   - 排除规则：`-j ACCEPT`（内核态直接允许，跳过后续过滤）。

带sni语义的算法, 默认bm算法升级版
-m sni --str 



## 六、关键约束（适配路由器资源）
1. **规则数量**：全局/MAC模式合计≤16条，避免内核规则表过大；
2. **匹配范围**：仅匹配TCP/UDP载荷前64KB，覆盖URL/域名核心区域；
3. **算法选择**：优先使用bm算法（短字符串匹配效率高于AC算法）；
4. **性能监控**：内核态CPU占用≤2%，网络延迟≤0.1ms，无感知影响上网体验。

## 七、底层实现核心原则
1. **用户空间做全量计算**：协议解析、格式转换、规则合并、算法选择全部在ASP页面/Shell脚本中完成；
2. **内核空间仅执行精简规则**：仅下发最终iptables命令，无任何复杂逻辑；
3. **复用原生模块**：基于iptables的string/time/mac/ip模块，避免自定义内核模块，保证稳定性；
4. **热更新机制**：应用规则时仅重启防火墙模块，无需重启网络，规则即时生效。

## 八、兼容性说明
1. 内核需开启以下模块（Padavan默认支持）：
   - `CONFIG_NETFILTER_XT_MATCH_STRING=y`（字符串匹配）；
   - `CONFIG_NETFILTER_XT_MATCH_TIME=y`（时间匹配）；
   - `CONFIG_NETFILTER_XT_MATCH_MAC=y`（MAC匹配）；
   - `CONFIG_NETFILTER_XT_MATCH_IPRANGE=y`（IP网段匹配）。
2. 适配所有基于Padavan的路由器，中低端路由（如MT7628/MT7621）均可稳定运行。

