#!/bin/sh
#2024/01/10 by tekintian

# 调试函数 - 显示当前状态信息
debug_adbyby_status()
{
	logger -t "adbyby" "=== AdByBy 调试信息 ==="
	logger -t "adbyby" "adbyby_enable: $adbyby_enable"
	logger -t "adbyby" "PROG_PATH: $PROG_PATH"
	logger -t "adbyby" "adbyby_dir: $adbyby_dir"
	logger -t "adbyby" "DATA_PATH: $DATA_PATH"
	logger -t "adbyby" "程序文件检查: $(test -f "$PROG_PATH/adbyby" && echo "存在" || echo "不存在")"
	logger -t "adbyby" "目录检查: $(test -d "$adbyby_dir" && echo "存在" || echo "不存在")"
	logger -t "adbyby" "数据目录检查: $(test -d "$DATA_PATH" && echo "存在" || echo "不存在")"
	if [ -d "$adbyby_dir" ]; then
		logger -t "adbyby" "目录内容: $(ls -la $adbyby_dir 2>/dev/null | head -10)"
	fi
	if [ -d "$DATA_PATH" ]; then
		logger -t "adbyby" "数据目录内容: $(ls -la $DATA_PATH 2>/dev/null | head -10)"
	fi
	logger -t "adbyby" "=== 调试信息结束 ==="
}

# 初始化AdByBy环境
init_adbyby_env()
{
	logger -t "adbyby" "初始化AdByBy环境..."
	
	# 创建必要的目录结构
	mkdir -p /tmp/adbyby
	mkdir -p /tmp/adbyby/data
	mkdir -p /etc/storage/dnsmasq-adbyby.d
	mkdir -p /tmp/dnsmasq.d
	mkdir -p /etc/storage/cron/crontabs
	
	# 设置权限
	chmod -R 755 /tmp/adbyby 2>/dev/null
	chmod -R 755 /etc/storage/dnsmasq-adbyby.d 2>/dev/null
	
	# 创建基础配置文件（如果不存在）
	if [ ! -f "/tmp/adbyby/data/lazy.txt" ]; then
		echo "# AdByBy lazy rules" > /tmp/adbyby/data/lazy.txt
	fi
	
	if [ ! -f "/tmp/adbyby/data/video.txt" ]; then
		echo "# AdByBy video rules" > /tmp/adbyby/data/video.txt
	fi
	
	logger -t "adbyby" "AdByBy环境初始化完成"
}

adbyby_enable=`nvram get adbyby_enable`
adbyby_ip_x=`nvram get adbyby_ip_x`
adbyby_rules_x=`nvram get adbyby_rules_x`
adbyby_set=`nvram get adbyby_set`
http_username=`nvram get http_username`
adbyby_update=`nvram get adbyby_update`
adbyby_update_hour=`nvram get adbyby_update_hour`
adbyby_update_min=`nvram get adbyby_update_min`
nvram set adbyby_adb=0
ipt_n="iptables -t nat"
#adbyby_dir="/tmp/adbyby"
PROG_PATH="/usr/share/adbyby"
DATA_PATH="/tmp/adbyby/data"
adbyby_dir="/tmp/adbyby"
WAN_FILE="/etc/storage/dnsmasq-adbyby.d/03-adbyby-ipset.conf"
wan_mode=`nvram get adbyby_set`
#abp_mode=`nvram get adbyby_adb_update`
nvram set adbybyip_mac_x_0=""
nvram set adbybyip_ip_x_0=""
nvram set adbybyip_name_x_0=""
nvram set adbybyip_ip_road_x_0=""
nvram set adbybyrules_x_0=""
nvram set adbybyrules_road_x_0=""
adbyby_start()
{
	logger -t "adbyby" "开始启动AdByBy..."
	
	# 先初始化环境
	init_adbyby_env
	
	addscripts
	
	if [ ! -f "$PROG_PATH/adbyby" ]; then
		logger -t "adbyby" "adbyby程序文件不存在，请检查安装！"
		return 1
	fi
	logger -t "adbyby" "adbyby程序文件存在：$PROG_PATH/adbyby"
	
	# 创建配置文件符号链接（如果不存在）
	if [ ! -f "$adbyby_dir/adhook.ini" ]; then
		if [ -f "$PROG_PATH/adhook.ini" ]; then
			ln -sf $PROG_PATH/adhook.ini $adbyby_dir/adhook.ini
			logger -t "adbyby" "创建配置文件符号链接：$adbyby_dir/adhook.ini"
		else
			logger -t "adbyby" "警告：配置文件不存在 $PROG_PATH/adhook.ini"
		fi
	fi
	
	#if [ $abp_mode -eq 1 ]; then
	#$PROG_PATH/adblock.sh &
	#fi
	#$PROG_PATH/adupdate.sh &
	
	add_rules
	
	# 启动adbyby程序
	cd $adbyby_dir && $PROG_PATH/adbyby &>/dev/null &
	sleep 1
	
	# 检查程序是否启动成功
	if pgrep -f "adbyby" > /dev/null; then
		logger -t "adbyby" "AdByBy程序启动成功"
	else
		logger -t "adbyby" "AdByBy程序启动失败"
		return 1
	fi
	
	add_dns
	iptables-save | grep ADBYBY >/dev/null || \
	add_rule
	hosts_ads
	/sbin/restart_dhcpd
	add_cron
	logger -t "adbyby" "Adbyby启动完成。"
}

adbyby_close()
{
	del_rule
	del_cron
	del_dns
	killall -q adbyby
	if [ $mem_mode -eq 1 ]; then
	echo "stop mem mode"
	fi
	kill -9 $(ps | grep admem.sh | grep -v grep | awk '{print $1}') >/dev/null 2>&1 
	/sbin/restart_dhcpd
	logger -t "adbyby" "Adbyby已关闭。"

}

add_rules()
{
	logger -t "adbyby" "正在检查规则是否需要更新!"
	rm -f /tmp/adbyby/data/*.bak

	touch /tmp/local-md5.json && md5sum /tmp/adbyby/data/lazy.txt /tmp/adbyby/data/video.txt > /tmp/local-md5.json
	touch /tmp/md5.json && curl -k -s -o /tmp/md5.json --connect-timeout 5 --retry 3 https://gitee.com/tekintian/adt-rules/raw/master/adbyby/md5.json

	lazy_local=$(grep 'lazy' /tmp/local-md5.json | awk -F' ' '{print $1}')
	video_local=$(grep 'video' /tmp/local-md5.json | awk -F' ' '{print $1}')  
	lazy_online=$(sed  's/":"/\n/g' /tmp/md5.json  |  sed  's/","/\n/g' | sed -n '2p')
	video_online=$(sed  's/":"/\n/g' /tmp/md5.json  |  sed  's/","/\n/g' | sed -n '4p')

	if [ "$lazy_online"x != "$lazy_local"x -o "$video_online"x != "$video_local"x ]; then
	echo "MD5 not match! Need update!"
	logger -t "adbyby" "发现更新的规则,下载规则！"
	touch /tmp/lazy.txt && curl -k -s -o /tmp/lazy.txt --connect-timeout 5 --retry 3 https://gitee.com/tekintian/adt-rules/raw/master/adbyby/lazy.txt
	touch /tmp/video.txt && curl -k -s -o /tmp/video.txt --connect-timeout 5 --retry 3 https://gitee.com/tekintian/adt-rules/raw/master/adbyby/video.txt
	touch /tmp/local-md5.json && md5sum /tmp/lazy.txt /tmp/video.txt > /tmp/local-md5.json
	lazy_local=$(grep 'lazy' /tmp/local-md5.json | awk -F' ' '{print $1}')
	video_local=$(grep 'video' /tmp/local-md5.json | awk -F' ' '{print $1}')
	if [ "$lazy_online"x == "$lazy_local"x -a "$video_online"x == "$video_local"x ]; then
	echo "New rules MD5 match!"
	mv /tmp/lazy.txt /tmp/adbyby/data/lazy.txt
	mv /tmp/video.txt /tmp/adbyby/data/video.txt
	echo $(date +"%Y-%m-%d %H:%M:%S") > /tmp/adbyby.updated
	fi
	else
	echo "MD5 match! No need to update!"
	logger -t "adbyby" "没有更新的规则,本次无需更新！"
	fi

	rm -f /tmp/lazy.txt /tmp/video.txt /tmp/local-md5.json /tmp/md5.json
	logger -t "adbyby" "Adbyby规则更新完成"
	lazy_version=`head -1 /tmp/adbyby/data/lazy.txt | awk -F': ' '{print $2}'`
	video_version=`head -1 /tmp/adbyby/data/video.txt | awk -F': ' '{print $2}'`
	# 格式化时间显示：lazy.txt 的版本号转换为 YYYY-MM-DD HH:MM
	if [ ${#lazy_version} -eq 12 ]; then
		formatted_ltime=`echo $lazy_version | sed 's/\([0-9]\{4\}\)\([0-9]\{2\}\)\([0-9]\{2\}\)\([0-9]\{2\}\)\([0-9]\{2\}\)/\1-\2-\3 \4:\5/'`
	else
		formatted_ltime=$lazy_version
	fi
	# video.txt 的版本号处理
	if [ ${#video_version} -eq 8 ]; then
		formatted_vtime=`echo $video_version | sed 's/\([0-9]\{4\}\)\([0-9]\{2\}\)\([0-9]\{2\}\)/\1-\2-\3/'`
	else
		formatted_vtime=$video_version
	fi
	nvram set adbyby_ltime="$formatted_ltime"
	nvram set adbyby_vtime="$formatted_vtime"
	#nvram set adbyby_rules=`grep -v '^!' /tmp/adbyby/data/rules.txt | wc -l`

	#nvram set adbyby_utime=`cat /tmp/adbyby.updated 2>/dev/null`
	grep -v '^!' /etc/storage/adbyby_rules.sh | grep -v "^$" > $adbyby_dir/rules.txt
	grep -v '^!' /etc/storage/adbyby_blockip.sh | grep -v "^$" > $adbyby_dir/blockip.conf
	grep -v '^!' /etc/storage/adbyby_adblack.sh | grep -v "^$" > $adbyby_dir/adblack.conf
	grep -v '^!' /etc/storage/adbyby_adesc.sh | grep -v "^$" > $adbyby_dir/adesc.conf
	grep -v '^!' /etc/storage/adbyby_adhost.sh | grep -v "^$" > $adbyby_dir/adhost.conf
	logger -t "adbyby" "正在处理规则..."
	rm -f $DATA_PATH/user.bin
	rm -f $DATA_PATH/user.txt
	rulesnum=`nvram get adbybyrules_staticnum_x`
	if [ $adbyby_rules_x -eq 1 ]; then
	for i in $(seq 1 $rulesnum)
	do
		j=`expr $i - 1`
		rules_address=`nvram get adbybyrules_x$j`
		rules_road=`nvram get adbybyrules_road_x$j`
		if [ $rules_road -ne 0 ]; then
			logger -t "adbyby" "正在下载和合并第三方规则"
			curl -k -s -o /tmp/adbyby/user2.txt --connect-timeout 5 --retry 3 $rules_address
			grep -v '^!' /tmp/adbyby/user2.txt | grep -E '^(@@\||\||[[:alnum:]])' | sort -u | grep -v "^$" >> $DATA_PATH/user3adblocks.txt
			rm -f /tmp/adbyby/user2.txt
		fi
	done
	grep -v '^!' $DATA_PATH/user3adblocks.txt | grep -v "^$" >> $DATA_PATH/user.txt
	rm -f $DATA_PATH/user3adblocks.txt
	fi
	grep -v ^! $adbyby_dir/rules.txt >> $DATA_PATH/user.txt
	nvram set adbyby_user=`cat /tmp/adbyby/data/user.txt | wc -l`
}


add_cron()
{
	if [ $adbyby_update -eq 0 ]; then
	sed -i '/adbyby/d' /etc/storage/cron/crontabs/$http_username
	cat >> /etc/storage/cron/crontabs/$http_username << EOF
$adbyby_update_min $adbyby_update_hour * * * /bin/sh /usr/bin/adbyby.sh G >/dev/null 2>&1
EOF
	logger -t "adbyby" "设置每天$adbyby_update_hour时$adbyby_update_min分，自动更新规则！"
	fi
if [ $adbyby_update -eq 1 ]; then

	sed -i '/adbyby/d' /etc/storage/cron/crontabs/$http_username
	cat >> /etc/storage/cron/crontabs/$http_username << EOF
*/$adbyby_update_min */$adbyby_update_hour * * * /bin/sh /usr/bin/adbyby.sh G >/dev/null 2>&1
EOF
	logger -t "adbyby" "设置每隔$adbyby_update_hour时$adbyby_update_min分，自动更新规则！"
	fi
	if [ $adbyby_update -eq 2 ]; then
	sed -i '/adbyby/d' /etc/storage/cron/crontabs/$http_username
	fi
}

del_cron()
{
	sed -i '/adbyby/d' /etc/storage/cron/crontabs/$http_username
}

ip_rule()
{

	ipset -N adbyby_esc hash:ip
	$ipt_n -A ADBYBY -m set --match-set adbyby_esc dst -j RETURN
	num=`nvram get adbybyip_staticnum_x`
	if [ $adbyby_ip_x -eq 1 ]; then
	if [ $num -ne 0 ]; then
	logger -t "adbyby" "设置内网IP过滤控制"
	for i in $(seq 1 $num)
	do
		j=`expr $i - 1`
		ip=`nvram get adbybyip_ip_x$j`
		mode=`nvram get adbybyip_ip_road_x$j`
		case $mode in
		0)
			$ipt_n -A ADBYBY -s $ip -j RETURN
			logger -t "adbyby" "忽略$ip走AD过滤。"
			;;
		1)
			$ipt_n -A ADBYBY -s $ip -p tcp -j REDIRECT --to-ports 8118
			$ipt_n -A ADBYBY -s $ip -j RETURN
			logger -t "adbyby" "设置$ip走全局过滤。"
			;;
		2)
			ipset -N adbyby_wan hash:ip
			$ipt_n -A ADBYBY -m set --match-set adbyby_wan dst -s $ip -p tcp -j REDIRECT --to-ports 8118
			awk '!/^$/&&!/^#/{printf("ipset=/%s/'"adbyby_wan"'\n",$0)}' $adbyby_dir/adhost.conf > $WAN_FILE
			logger -t "adbyby" "设置$ip走Plus+过滤。"
			;;
		esac
	done
	fi
	fi

	case $wan_mode in
		0)	$ipt_n -A ADBYBY -p tcp -j REDIRECT --to-ports 8118
			;;
		1)
			ipset -N adbyby_wan hash:ip
			$ipt_n -A ADBYBY -m set --match-set adbyby_wan dst -p tcp -j REDIRECT --to-ports 8118
			;;
		2)
			$ipt_n -A ADBYBY -d 0.0.0.0/24 -j RETURN
			;;
	esac

	echo "create blockip hash:net family inet hashsize 1024 maxelem 65536" > /tmp/blockip.ipset
	awk '!/^$/&&!/^#/{printf("add blockip %s'" "'\n",$0)}' $adbyby_dir/blockip.conf >> /tmp/blockip.ipset
	ipset -! restore < /tmp/blockip.ipset 2>/dev/null
	iptables -I FORWARD -m set --match-set blockip dst -j DROP
	iptables -I OUTPUT -m set --match-set blockip dst -j DROP
}

add_dns()
{
	mkdir -p /etc/storage/dnsmasq-adbyby.d
	mkdir -p /tmp/dnsmasq.d
	anti_ad
	block_ios=`nvram get block_ios`
	block_shortvideo=`nvram get block_shortvideo`
	block_games=`nvram get block_games`
	awk '!/^$/&&!/^#/{printf("ipset=/%s/'"adbyby_esc"'\n",$0)}' $adbyby_dir/adesc.conf > /etc/storage/dnsmasq-adbyby.d/06-dnsmasq.esc
	awk '!/^$/&&!/^#/{printf("address=/%s/'"0.0.0.0"'\n",$0)}' $adbyby_dir/adblack.conf > /etc/storage/dnsmasq-adbyby.d/07-dnsmasq.black
	[ $block_ios -eq 1 ] && cat <<-EOF >> /etc/storage/dnsmasq-adbyby.d/07-dnsmasq.black
# Apple iOS OTA Update Blocking (Valid domains only)
address=/mesu.apple.com/0.0.0.0
address=/appldnld.apple.com/0.0.0.0
address=/updates-http.cdn-apple.com/0.0.0.0
address=/xp.apple.com/0.0.0.0
address=/gs.apple.com/0.0.0.0
address=/iosapps.itunes.apple.com/0.0.0.0
EOF
	if [ $block_shortvideo -eq 1 ]; then
  cat <<-EOF >/etc/storage/dnsmasq-adbyby.d/08-dnsmasq.shortvideo
# 热门短视频平台域名拦截规则

# 抖音相关域名 (Douyin/TikTok)
address=/.douyin.com/0.0.0.0
address=/.douyinvod.com/0.0.0.0
address=/.douyincdn.com/0.0.0.0
address=/.tiktok.com/0.0.0.0
address=/.tiktokcdn.com/0.0.0.0
address=/.tiktokv.com/0.0.0.0

# 字节跳动相关域名 (ByteDance)
address=/.bytedance.com/0.0.0.0
address=/.toutiao.com/0.0.0.0
address=/.snssdk.com/0.0.0.0
address=/.amemv.com/0.0.0.0
address=/.bytecdn.com/0.0.0.0
address=/.ibytecdn.com/0.0.0.0
address=/.sglog.com/0.0.0.0

# 快手相关域名 (Kuaishou)
address=/.kuaishou.com/0.0.0.0
address=/.kwimgs.com/0.0.0.0
address=/.ksad.com/0.0.0.0
address=/.kuaishou.com.cn/0.0.0.0
address=/.ksyungslb.com/0.0.0.0
address=/.yximgs.com/0.0.0.0

# 微信视频号相关域名 (WeChat Video)
address=/.channels.weixin.qq.com/0.0.0.0
address=/.weixin.qq.com/0.0.0.0
address=/.wx.qq.com/0.0.0.0
address=/.wx.qlogo.cn/0.0.0.0
address=/.video.qq.com/0.0.0.0

# 小红书相关域名 (Xiaohongshu/RED)
address=/.xiaohongshu.com/0.0.0.0
address=/.xhslink.com/0.0.0.0
address=/.xhsjpg.com/0.0.0.0
address=/.xhsdsp.com/0.0.0.0
address=/.redbook.com/0.0.0.0

# QQ短视频相关域名 (QQ Video)
address=/.v.qq.com/0.0.0.0
address=/.video.qq.com/0.0.0.0
address=/.liveplay.qq.com/0.0.0.0
address=/.video.gtimg.cn/0.0.0.0

# 西瓜视频相关域名 (Xigua Video)
address=/.ixigua.com/0.0.0.0
address=/.toutiao.com/0.0.0.0
address=/.snssdk.com/0.0.0.0

# 美拍相关域名 (Meipai)
address=/.meipai.com/0.0.0.0
address=/.meitu.com/0.0.0.0
address=/.meitudata.com/0.0.0.0

# 火山小视频相关域名 (Huoshan)
address=/.huoshan.com/0.0.0.0
address=/.huoshan.tv/0.0.0.0

# 梨视频相关域名 (Pear Video)
address=/.pearvideo.com/0.0.0.0
address=/.pearnode.com/0.0.0.0

# 微视相关域名 (Weishi)
address=/.weishi.com/0.0.0.0
address=/.weishi.qq.com/0.0.0.0

# 相关分析和跟踪域名
address=/.sglog.com/0.0.0.0
address=/.tencent.com/0.0.0.0
address=/.qzone.qq.com/0.0.0.0
		EOF
	fi
	if [ $block_games -eq 1 ]; then
  cat <<-EOF >/etc/storage/dnsmasq-adbyby.d/09-dnsmasq.games
# Popular Online Games Blocking (Valid domains only)
# Minecraft (我的世界)
address=/.minecraft.net/0.0.0.0
address=/session.minecraft.net/0.0.0.0

# Tencent Games (腾讯游戏)
address=/.pvp.qq.com/0.0.0.0
address=/.game.qq.com/0.0.0.0

# Steam Games
address=/.steampowered.com/0.0.0.0
address=/.steamcommunity.com/0.0.0.0

# NetEase Games (网易游戏)
address=/.game.163.com/0.0.0.0
address=/.nie.163.com/0.0.0.0

# PUBG
address=/.pubg.com/0.0.0.0

# Epic Games
address=/.epicgames.com/0.0.0.0
EOF
	fi
	sed -i '/dnsmasq-adbyby/d' /etc/storage/dnsmasq/dnsmasq.conf
	cat >> /etc/storage/dnsmasq/dnsmasq.conf << EOF
conf-dir=/etc/storage/dnsmasq-adbyby.d
EOF
	if [ $wan_mode -eq 1 ]; then
	awk '!/^$/&&!/^#/{printf("ipset=/%s/'"adbyby_wan"'\n",$0)}' $PROG_PATH/adhost.conf > $WAN_FILE
	fi
	if ls /etc/storage/dnsmasq-adbyby.d/* >/dev/null 2>&1; then
	mkdir -p /tmp/dnsmasq.d
	fi
}

del_dns()
{
	sed -i '/dnsmasq-adbyby/d' /etc/storage/dnsmasq/dnsmasq.conf
	sed -i '/hosts/d' /etc/storage/dnsmasq/dnsmasq.conf
	rm -f /tmp/dnsmasq.d/dnsmasq-adbyby.conf
	rm -f /etc/storage/dnsmasq-adbyby.d/*
	rm -f /tmp/adbyby_host.conf
}


add_rule()
{
	$ipt_n -N ADBYBY
	$ipt_n -A ADBYBY -d 0.0.0.0/8 -j RETURN
	$ipt_n -A ADBYBY -d 10.0.0.0/8 -j RETURN
	$ipt_n -A ADBYBY -d 127.0.0.0/8 -j RETURN
	$ipt_n -A ADBYBY -d 169.254.0.0/16 -j RETURN
	$ipt_n -A ADBYBY -d 172.16.0.0/12 -j RETURN
	$ipt_n -A ADBYBY -d 192.168.0.0/16 -j RETURN
	$ipt_n -A ADBYBY -d 224.0.0.0/4 -j RETURN
	$ipt_n -A ADBYBY -d 240.0.0.0/4 -j RETURN
	ip_rule
	logger -t "adbyby" "添加8118透明代理端口。"
	$ipt_n -I PREROUTING -p tcp --dport 80 -j ADBYBY
	iptables-save | grep -E "ADBYBY|^\*|^COMMIT" | sed -e "s/^-A \(OUTPUT\|PREROUTING\)/-I \1 1/" > /tmp/adbyby.save
	if [ -f "/tmp/adbyby.save" ]; then
	logger -t "adbyby" "保存adbyby防火墙规则成功！"
	else
	logger -t "adbyby" "保存adbyby防火墙规则失败！可能会造成重启后过滤广告失效，需要手动关闭再打开ADBYBY！"
	fi
}

del_rule()
{
	$ipt_n -D PREROUTING -p tcp --dport 80 -j ADBYBY 2>/dev/null
	$ipt_n -F ADBYBY 2>/dev/null
	$ipt_n -X ADBYBY 2>/dev/null
	iptables -D FORWARD -m set --match-set blockip dst -j DROP 2>/dev/null
	iptables -D OUTPUT -m set --match-set blockip dst -j DROP 2>/dev/null
	ipset -F adbyby_esc 2>/dev/null
	ipset -X adbyby_esc 2>/dev/null
	ipset -F adbyby_wan 2>/dev/null
	ipset -X adbyby_wan 2>/dev/null
	ipset -F blockip 2>/dev/null
	ipset -X blockip 2>/dev/null
	logger -t "adbyby" "已关闭全部8118透明代理端口。"
}

reload_rule()
{
	config_load adbyby
	config_foreach get_config adbyby
	del_rule
	iptables-save | grep ADBYBY >/dev/null || \
	add_rule
}

adbyby_uprules()
{
	logger -t "adbyby" "开始更新AdByBy规则..."
	
	adbyby_close
	# 先初始化环境
	init_adbyby_env
	addscripts
	
	if [ ! -f "$PROG_PATH/adbyby" ]; then
		logger -t "adbyby" "adbyby程序文件不存在，请检查安装！"
		return 1
	fi
	logger -t "adbyby" "adbyby程序文件存在：$PROG_PATH/adbyby"
	
	# 创建配置文件符号链接（如果不存在）
	if [ ! -f "$adbyby_dir/adhook.ini" ]; then
		if [ -f "$PROG_PATH/adhook.ini" ]; then
			ln -sf $PROG_PATH/adhook.ini $adbyby_dir/adhook.ini
			logger -t "adbyby" "创建配置文件符号链接：$adbyby_dir/adhook.ini"
		else
			logger -t "adbyby" "警告：配置文件不存在 $PROG_PATH/adhook.ini"
		fi
	fi
	
	#if [ $abp_mode -eq 1 ]; then
	#$PROG_PATH/adblock.sh &
	#fi
	#$PROG_PATH/adupdate.sh &
	
	add_rules
	
	# 启动adbyby程序
	cd $adbyby_dir && $PROG_PATH/adbyby &>/dev/null &
	sleep 1
	
	# 检查程序是否启动成功
	if pgrep -f "adbyby" > /dev/null; then
		logger -t "adbyby" "AdByBy程序启动成功"
	else
		logger -t "adbyby" "AdByBy程序启动失败"
		return 1
	fi
	
	add_dns
	iptables-save | grep ADBYBY >/dev/null || \
	add_rule
	hosts_ads
	/sbin/restart_dhcpd
	#add_cron
	logger -t "adbyby" "AdByBy规则更新完成。"
}

#updateadb()
#{
#	/tmp/adbyby/adblock.sh &
#}
anti_ad(){
anti_ad=`nvram get anti_ad`
anti_ad_link=`nvram get anti_ad_link`
nvram set anti_ad_count=0
if [ "$anti_ad" = "1" ]; then
curl -k -s -o /etc/storage/dnsmasq-adbyby.d/anti-ad-for-dnsmasq.conf --connect-timeout 5 --retry 3 $anti_ad_link
if [ ! -f "/etc/storage/dnsmasq-adbyby.d/anti-ad-for-dnsmasq.conf" ]; then
	logger -t "adbyby" "anti_AD下载失败！"
else
	logger -t "adbyby" "anti_AD下载成功,处理中..."
nvram set anti_ad_count=`grep -v '^#' /etc/storage/dnsmasq-adbyby.d/anti-ad-for-dnsmasq.conf | wc -l`
fi
fi
}

hosts_ads(){
adbyby_hosts=`nvram get hosts_ad`
nvram set adbyby_hostsad=0
if [ "$adbyby_hosts" = "1" ]; then
rm -rf $PROG_PATH/hosts
if [ -f "/etc/storage/adbyby_host.sh" ] && [ -s "/etc/storage/adbyby_host.sh" ]; then
	grep -v '^#' /etc/storage/adbyby_host.sh | grep -v "^$" > $PROG_PATH/hostlist.txt
	if [ -s "$PROG_PATH/hostlist.txt" ]; then
		for ip in `cat $PROG_PATH/hostlist.txt`
		do
		logger -t "adbyby" "正在下载: $ip"
		curl -k -s -o /tmp/host.txt --connect-timeout 5 --retry 3 $ip
		if [ ! -f "/tmp/host.txt" ]; then
			logger -t "adbyby" "$ip 下载失败！"
		else
			logger -t "adbyby" "hosts下载成功,处理中..."
			grep -v '^#' /tmp/host.txt | grep -v "^$" >> $PROG_PATH/hosts
		fi
		done
		rm -f /tmp/host.txt
		logger -t "adbyby" "正在对hosts文件进行去重处理."
		if [ -f "$PROG_PATH/hosts" ]; then
			sort $PROG_PATH/hosts | uniq > $PROG_PATH/hosts.tmp && mv $PROG_PATH/hosts.tmp $PROG_PATH/hosts
			nvram set adbyby_hostsad=`grep -v '^!' $PROG_PATH/hosts | wc -l`
			sed -i '/hosts/d' /etc/storage/dnsmasq/dnsmasq.conf
			cat >> /etc/storage/dnsmasq/dnsmasq.conf <<-EOF
	addn-hosts=$PROG_PATH/hosts
EOF
		fi
	else
		logger -t "adbyby" "hosts下载列表为空，跳过hosts处理"
	fi
else
	logger -t "adbyby" "hosts配置文件不存在，请先配置hosts下载列表"
fi
else
	# 移除dnsmasq中的hosts配置
	sed -i '/hosts/d' /etc/storage/dnsmasq/dnsmasq.conf
	rm -f $PROG_PATH/hosts
fi
rm -f $PROG_PATH/hostlist.txt
}


addscripts()
{

	adbyby_rules="/etc/storage/adbyby_rules.sh"
	if [ ! -f "$adbyby_rules" ] || [ ! -s "$adbyby_rules" ] ; then
	cat > "$adbyby_rules" <<-EOF
!  ------------------------------ ADByby 自定义过滤语法简表---------------------------------
!  "!" 为行注释符，注释行以该符号起始作为一行注释语义，用于规则描述
!  "*" 为字符通配符，能够匹配0长度或任意长度的字符串，该通配符不能与正则语法混用。
!  "^" 为分隔符，可以是除了字母、数字或者 _ - . % 之外的任何字符。
!  "|" 为管线符号，来表示地址的最前端或最末端
!  "||" 为子域通配符，方便匹配主域名下的所有子域。
!  "~" 为排除标识符，通配符能过滤大多数广告，但同时存在误杀, 可以通过排除标识符修正误杀链接。
!  "##" 为元素选择器标识符，后面跟需要隐藏元素的CSS样式例如 #ad_id  .ad_class
!!  元素隐藏暂不支持全局规则和排除规则
!! 字符替换扩展
!  文本替换选择器标识符，后面跟需要替换的文本数据，格式：$s@模式字符串@替换后的文本@
!  支持通配符*和？
!  -------------------------------------------------------------------------------------------

EOF
	chmod 755 "$adbyby_rules"
	fi

	adbyby_blockip="/etc/storage/adbyby_blockip.sh"
	if [ ! -f "$adbyby_blockip" ] || [ ! -s "$adbyby_blockip" ] ; then
	cat > "$adbyby_blockip" <<-EOF
2.2.2.2

EOF
	chmod 755 "$adbyby_blockip"
	fi

	adbyby_adblack="/etc/storage/adbyby_adblack.sh"
	if [ ! -f "$adbyby_adblack" ] || [ ! -s "$adbyby_adblack" ] ; then
	cat > "$adbyby_adblack" <<-EOF
pogothere.xyz
evidenceguidance.com
config.kuyun.com

EOF
	chmod 755 "$adbyby_adblack"
	fi

	adbyby_adesc="/etc/storage/adbyby_adesc.sh"
	if [ ! -f "$adbyby_adesc" ] || [ ! -s "$adbyby_adesc" ] ; then
	cat > "$adbyby_adesc" <<-EOF
weixin.qq.com
qpic.cn
imtt.qq.com
api.codebuddy.com
api.codebuddy.com
chat.openai.com
api.openai.com
openai.com
cdn.openai.com

EOF
	chmod 755 "$adbyby_adesc"
	fi

	adbyby_adhost="/etc/storage/adbyby_adhost.sh"
	if [ ! -f "$adbyby_adhost" ] || [ ! -s "$adbyby_adhost" ] ; then
	cat > "$adbyby_adhost" <<-EOF
analytics-union.xunlei.com
mediav.com
doubleclick.net
admaster.com.cn
serving-sys.com

EOF
	chmod 755 "$adbyby_adhost"
	fi

	adbyby_host="/etc/storage/adbyby_host.sh"
	if [ ! -f "$adbyby_host" ] || [ ! -s "$adbyby_host" ] ; then
	cat > "$adbyby_host" <<-EOF
# AdByby Hosts下载列表配置文件
# 每行一个URL，支持http/https协议
# 以下是一些常用的hosts源示例（默认注释掉，请根据需要启用）

# 常见广告过滤hosts
https://gitee.com/tekintian/adt-rules/raw/master/hosts/ads_hosts.txt

# 统计站点过滤hosts
# https://gitee.com/tekintian/adt-rules/raw/master/hosts/stats_hosts.txt

# adaway[https://adaway.org/hosts.txt]精简版
# https://gitee.com/tekintian/adt-rules/raw/master/hosts/adaway_hosts.txt


EOF
	chmod 755 "$adbyby_host"
	fi
}
case $1 in
start)
adbyby_start
;;
stop)
adbyby_close
;;
A)
add_rules
;;
C)
add_rule
;;
D)
add_dns
;;
E)
addscripts
;;
F)
hosts_ads
;;
G)
adbyby_uprules
;;
debug)
debug_adbyby_status
;;
init)
init_adbyby_env
;;
*)
echo "Usage: $0 {start|stop|A|C|D|E|F|G|debug|init}"
echo "  start  - 启动AdByBy服务"
echo "  stop   - 停止AdByBy服务"
echo "  A      - 更新规则"
echo "  C      - 添加防火墙规则"
echo "  D      - 添加DNS规则"
echo "  E      - 添加脚本文件"
echo "  F      - 更新hosts文件"
echo "  G      - 更新规则并重启"
echo "  debug  - 显示调试信息"
echo "  init   - 初始化环境"
;;
esac
