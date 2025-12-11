# AdByBy anti-AD规则下载列表配置文件
# 每行一个URL，支持http/https协议
# 下载的规则文件会被合并、去重后生成anti-ad-for-dnsmasq.conf
# 注意, 文件里面的必须是dnsmasq规则格式,如: address=/domain.com/0.0.0.0
# 匹配域名 + 所有子域名 → address=/domain.com/0.0.0.0
# 加 ^ 表示精确匹配（不匹配子域名） → address=/^domain.com/0.0.0.0

# Adbyby项目默认dnsmasq规则源
https://gitee.com/tekintian/adt-rules/raw/master/dnsmasq/anti-ad.conf

# 游戏相关规则
# https://gitee.com/tekintian/adt-rules/raw/master/dnsmasq/games.conf
# 电商平台广告规则
# https://gitee.com/tekintian/adt-rules/raw/master/dnsmasq/shop.conf
