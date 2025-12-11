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

# 检查8118代理状态
debug_8118_status()
{
	logger -t "adbyby" "=== 8118代理状态检查 ==="
	
	# 检查adbyby进程
	if pgrep -f "adbyby" > /dev/null; then
		ADBYBY_PID=$(pgrep -f "adbyby")
		logger -t "adbyby" "AdByBy进程运行中，PID: $ADBYBY_PID"
		
		# 检查进程状态
		if ps -p $ADBYBY_PID > /dev/null 2>&1; then
			logger -t "adbyby" "AdByBy进程状态正常"
		else
			logger -t "adbyby" "AdByBy进程状态异常"
		fi
		
		# 检查端口监听
		if netstat -ln | grep ":8118" > /dev/null 2>&1; then
			logger -t "adbyby" "8118端口正在监听"
			netstat -ln | grep ":8118" | while read line; do
				logger -t "adbyby" "端口状态: $line"
			done
		else
			logger -t "adbyby" "8118端口未在监听"
		fi
		
		# 检查配置文件
		if [ -f "/tmp/adbyby/adhook.ini" ]; then
			logger -t "adbyby" "配置文件存在: /tmp/adbyby/adhook.ini"
			if grep -q "listen-address=0.0.0.0:8118" "/tmp/adbyby/adhook.ini"; then
				logger -t "adbyby" "配置文件监听地址正确"
			else
				logger -t "adbyby" "配置文件监听地址异常"
			fi
		else
			logger -t "adbyby" "配置文件不存在: /tmp/adbyby/adhook.ini"
		fi
		
		# 检查防火墙规则
		if iptables -t nat -L PREROUTING | grep ADBYBY > /dev/null 2>&1; then
			logger -t "adbyby" "防火墙规则存在"
		else
			logger -t "adbyby" "防火墙规则不存在"
		fi
		
		# 注意：状态页面功能已移除，不再进行HTTP请求测试
		# 只检查端口监听状态，避免TIME_WAIT连接
		
	else
		logger -t "adbyby" "AdByBy进程未运行"
	fi
	
	logger -t "adbyby" "=== 8118代理状态检查结束 ==="
}

# 智能健康检查和自动重启adbyby - 自适应频率（移除状态页面依赖）
HEALTH_STATE_FILE="/tmp/adbyby_health.state"
CONSECUTIVE_FAILURES=0
MAX_FAILURES=3

# 获取当前检查间隔（分钟）
get_health_check_interval()
{
	local failures=$(cat $HEALTH_STATE_FILE 2>/dev/null || echo "0")
	case $failures in
		0) echo "60" ;;    # 正常状态：1小时检查一次
		1) echo "30" ;;    # 1次失败：30分钟检查一次  
		2) echo "15" ;;    # 2次失败：15分钟检查一次
		*) echo "5"  ;;    # 3+次失败：5分钟检查一次
	esac
}

# 更新健康状态
update_health_state()
{
	local status=$1
	local current=$(cat $HEALTH_STATE_FILE 2>/dev/null || echo "0")
	
	case $status in
		"success")
			# 成功则重置失败计数
			echo "0" > $HEALTH_STATE_FILE
			CONSECUTIVE_FAILURES=0
			;;
		"failure")
			# 失败则增加计数
			CONSECUTIVE_FAILURES=$((current + 1))
			echo $CONSECUTIVE_FAILURES > $HEALTH_STATE_FILE
			;;
	esac
}

# 轻量级进程检查（比pgrep更高效）
is_adbyby_running()
{
	# 检查多个可能的PID文件位置
	local pid_files="/var/run/adbyby.pid /tmp/adbyby.pid /tmp/adbyby/adbyby.pid"
	
	for pid_file in $pid_files; do
		if [ -f "$pid_file" ]; then
			local pid=$(cat "$pid_file" 2>/dev/null)
			if [ -n "$pid" ] && [ -d "/proc/$pid" ] 2>/dev/null; then
				return 0
			fi
		fi
	done
	
	# 回退到进程检查
	pgrep -f "adbyby" > /dev/null 2>/dev/null
}

# 极简端口检查（兼容多种路由器环境）
is_8118_listening()
{
	# 优先使用/proc/net/tcp（最底层，最可靠，避免调用外部命令）
	if [ -f "/proc/net/tcp" ]; then
		# 8118的十六进制是1FB6，直接检查
		grep "1FB6" /proc/net/tcp >/dev/null 2>&1 && return 0
	fi
	
	# 备用方案：使用netstat
	if command -v netstat >/dev/null 2>&1; then
		# 精确匹配8118端口，避免误匹配81180等
		# 检查 ":8118 " (带空格) 或 ":8118$" (行尾)
		if netstat -ln 2>/dev/null | grep ":8118 " >/dev/null 2>&1; then
			return 0
		fi
		if netstat -ln 2>/dev/null | grep ":8118$" >/dev/null 2>&1; then
			return 0
		fi
	fi
	
	return 1
}

health_check_adbyby()
{
	local service_ok=1
	local failure_reason=""
	
	# 快速检查：先看PID文件
	if ! is_adbyby_running; then
		service_ok=0
		failure_reason="进程不存在"
	else
		# 进程存在，检查端口（使用更高效的ss命令）
		if ! is_8118_listening; then
			service_ok=0
			failure_reason="进程存在但端口未监听"
		fi
	fi
	
	if [ $service_ok -eq 1 ]; then
		# 服务正常
		update_health_state "success"
		
		# 只有从失败状态恢复时才记录日志
		local prev_failures=$(cat $HEALTH_STATE_FILE 2>/dev/null || echo "0")
		if [ $prev_failures -gt 0 ]; then
			logger -t "adbyby" "健康检查：服务已恢复正常（之前连续失败$prev_failures次）"
		fi
	else
		# 服务异常
		logger -t "adbyby" "健康检查：$failure_reason，第$((CONSECUTIVE_FAILURES + 1))次失败，重启服务"
		
		# 强制清理可能的僵尸进程
		killall -9 -q adbyby 2>/dev/null
		# 清理PID文件
		cleanup_pid_files
		sleep 1
		
		# 重新启动
		cd $adbyby_dir && $PROG_PATH/adbyby &>/dev/null &
		sleep 2
		
		# 验证启动是否成功
		if is_adbyby_running && is_8118_listening; then
			logger -t "adbyby" "健康检查：服务重启成功"
		else
			logger -t "adbyby" "健康检查：服务重启失败，需要人工干预"
		fi
		
		update_health_state "failure"
	fi
	
	# 动态调整下次检查时间
	local interval=$(get_health_check_interval)
	echo "下次检查间隔：$interval 分钟"
	
	# 更新crontab中的检查频率
	update_cron_interval $interval
}

# 动态更新crontab检查频率
update_cron_interval()
{
	local interval=$1
	local cron_entry="*/$interval * * * * /bin/sh /usr/bin/adbyby.sh health_check >/dev/null 2>&1"
	
	# 移除旧的health_check条目
	sed -i '/health_check/d' /etc/storage/cron/crontabs/$http_username 2>/dev/null
	
	# 添加新的条目
	echo "$cron_entry" >> /etc/storage/cron/crontabs/$http_username 2>/dev/null
}

# 注意：cleanup_8118_connections函数已移除，功能已整合到health_check中

# 初始化AdByBy环境
init_adbyby_env()
{
	logger -t "adbyby" "初始化AdByBy环境..."
	
	# 创建必要的目录结构
	mkdir -p /tmp/adbyby/data
	mkdir -p /etc/storage/dnsmasq-adbyby.d
	mkdir -p /tmp/dnsmasq.d
	mkdir -p /etc/storage/cron/crontabs
	mkdir -p /var/run
	
	# 设置权限
	chmod -R 755 /tmp/adbyby 2>/dev/null
	chmod -R 755 /etc/storage/dnsmasq-adbyby.d 2>/dev/null
	
	# 创建基础配置文件（如果不存在）
	if [ ! -f "/tmp/adbyby/data/lazy.txt" ]; then
		cp /usr/share/adbyby/data/lazy.txt /tmp/adbyby/data/lazy.txt
		logger -t "adbyby" "拷贝data/lazy.txt规则文件"
	fi
	
	if [ ! -f "/tmp/adbyby/data/video.txt" ]; then
		cp /usr/share/adbyby/data/video.txt /tmp/adbyby/data/video.txt
		logger -t "adbyby" "拷贝data/video.txt规则文件"
	fi
	
	# 创建rules.txt文件（如果不存在）
	if [ ! -f "/tmp/adbyby/data/rules.txt" ]; then
		cp /usr/share/adbyby/data/rules.txt /tmp/adbyby/data/rules.txt 2>/dev/null || {
			# 如果源文件不存在，创建空文件
			touch /tmp/adbyby/data/rules.txt
			logger -t "adbyby" "创建空的rules.txt文件"
		}
		logger -t "adbyby" "确保rules.txt文件存在"
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

# 参数验证，确保数值类型
[ -z "$adbyby_enable" ] && adbyby_enable=0
[ -z "$adbyby_ip_x" ] && adbyby_ip_x=0
[ -z "$adbyby_rules_x" ] && adbyby_rules_x=0
[ -z "$adbyby_set" ] && adbyby_set=1
[ -z "$adbyby_update" ] && adbyby_update=2
[ -z "$adbyby_update_hour" ] && adbyby_update_hour=3
[ -z "$adbyby_update_min" ] && adbyby_update_min=30
mem_mode=0
# 移除未使用的adbyby_adb变量
ipt_n="iptables -t nat"
PROG_PATH="/usr/share/adbyby"
DATA_PATH="/tmp/adbyby/data"
adbyby_dir="/tmp/adbyby"
WAN_FILE="/etc/storage/dnsmasq-adbyby.d/03-adbyby-ipset.conf"
wan_mode=`nvram get adbyby_set`
# 移除未使用的nvram变量初始化
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

	add_rules
	
	# 启动adbyby程序
	cd $adbyby_dir && $PROG_PATH/adbyby &>/dev/null &
	sleep 2
	
	# 检查程序是否启动成功（优先检查PID文件）
	local startup_success=0
	if is_adbyby_running; then
		logger -t "adbyby" "AdByBy程序启动成功（PID文件检查）"
		startup_success=1
	elif pgrep -f "adbyby" > /dev/null; then
		logger -t "adbyby" "AdByBy程序启动成功（进程检查）"
		startup_success=1
	else
		logger -t "adbyby" "AdByBy程序启动失败"
		return 1
	fi
	
	if [ $startup_success -eq 1 ]; then
		# 初始化健康状态
		echo "0" > $HEALTH_STATE_FILE
		# 额外等待确保程序完全初始化
		sleep 1
	fi
	
	add_dns
	iptables-save | grep ADBYBY >/dev/null || \
	add_rule
	hosts_ads
	/sbin/restart_dhcpd
	add_cron
	
	# 启动智能健康检查（自适应频率，默认1小时）
	echo "*/60 * * * * /bin/sh /usr/bin/adbyby.sh health_check >/dev/null 2>&1" >> /etc/storage/cron/crontabs/$http_username 2>/dev/null
	logger -t "adbyby" "已添加adbyby智能健康检查任务（自适应频率）"
	
	logger -t "adbyby" "Adbyby启动完成。"
}

# 清理所有可能的PID文件
cleanup_pid_files()
{
	local pid_files="/var/run/adbyby.pid /tmp/adbyby.pid /tmp/adbyby/adbyby.pid"
	
	for pid_file in $pid_files; do
		if [ -f "$pid_file" ]; then
			rm -f "$pid_file" 2>/dev/null
			logger -t "adbyby" "清理PID文件: $pid_file"
		fi
	done
}

adbyby_close()
{
	del_rule
	del_cron
	del_dns
	killall -q adbyby
	# 清理PID文件
	cleanup_pid_files
	/sbin/restart_dhcpd
	logger -t "adbyby" "Adbyby已关闭。"

}

add_rules()
{
	logger -t "adbyby" "正在检查规则是否需要更新!"
	
	# 确保规则文件存在，如果不存在则创建默认文件
	if [ ! -f "/tmp/adbyby/data/lazy.txt" ] || [ ! -f "/tmp/adbyby/data/video.txt" ] ; then
		logger -t "adbyby" "lazy.txt或video.txt文件不存在,初始化AdByBy环境!"
		init_adbyby_env
	fi

	rm -f /tmp/adbyby/data/*.bak

	touch /tmp/local-md5.json && md5sum /tmp/adbyby/data/lazy.txt /tmp/adbyby/data/video.txt > /tmp/local-md5.json
	touch /tmp/md5.json && curl -k -s -o /tmp/md5.json --connect-timeout 5 --retry 3 https://gitee.com/tekintian/adt-rules/raw/master/adbyby/md5.json

	lazy_local=$(grep 'lazy' /tmp/local-md5.json | awk -F' ' '{print $1}')
	video_local=$(grep 'video' /tmp/local-md5.json | awk -F' ' '{print $1}')
	# 从md5.json文件中提取lazy.txt和video.txt的MD5值
	lazy_online=$(grep 'lazy.txt' /tmp/md5.json | awk -F'"' '{print $4}')
	video_online=$(grep 'video.txt' /tmp/md5.json | awk -F'"' '{print $4}')

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
	# 检查规则文件是否存在并获取版本信息
	if [ -f "/tmp/adbyby/data/lazy.txt" ] && [ -s "/tmp/adbyby/data/lazy.txt" ]; then
		lazy_version=`head -1 /tmp/adbyby/data/lazy.txt | awk -F': ' '{print $2}'`
		# 格式化时间显示：lazy.txt 的版本号转换为 YYYY-MM-DD HH:MM
		if [ ${#lazy_version} -eq 12 ]; then
			formatted_ltime=`echo $lazy_version | sed 's/\([0-9]\{4\}\)\([0-9]\{2\}\)\([0-9]\{2\}\)\([0-9]\{2\}\)\([0-9]\{2\}\)/\1-\2-\3 \4:\5/'`
		else
			formatted_ltime=$lazy_version
		fi
	else
		formatted_ltime="规则未下载"
		logger -t "adbyby" "警告：lazy.txt 规则文件不存在或为空"
	fi
	
	if [ -f "/tmp/adbyby/data/video.txt" ] && [ -s "/tmp/adbyby/data/video.txt" ]; then
		video_version=`head -1 /tmp/adbyby/data/video.txt | awk -F': ' '{print $2}'`
		# video.txt 的版本号处理
		if [ ${#video_version} -eq 8 ]; then
			formatted_vtime=`echo $video_version | sed 's/\([0-9]\{4\}\)\([0-9]\{2\}\)\([0-9]\{2\}\)/\1-\2-\3/'`
		else
			formatted_vtime=$video_version
		fi
	else
		formatted_vtime="规则未下载"
		logger -t "adbyby" "警告：video.txt 规则文件不存在或为空"
	fi
	
	nvram set adbyby_ltime="$formatted_ltime"
	nvram set adbyby_vtime="$formatted_vtime"
	logger -t "adbyby" "规则版本更新 - 静态规则：$formatted_ltime | 视频规则：$formatted_vtime"
	nvram set adbyby_rules=`grep -v '^!' /tmp/adbyby/data/rules.txt | wc -l`

	nvram set adbyby_utime=`cat /tmp/adbyby.updated 2>/dev/null`
	grep -v '^!' /etc/storage/adbyby_rules.sh | grep -v "^$" > $adbyby_dir/rules.txt
	grep -v '^!' /etc/storage/adbyby_blockip.sh | grep -v "^$" > $adbyby_dir/blockip.conf
	grep -v '^!' /etc/storage/adbyby_adblack.sh | grep -v "^$" > $adbyby_dir/adblack.conf
	grep -v '^!' /etc/storage/adbyby_adesc.sh | grep -v "^$" > $adbyby_dir/adesc.conf
	grep -v '^!' /etc/storage/adbyby_adhost.sh | grep -v "^$" > $adbyby_dir/adhost.conf
	logger -t "adbyby" "正在处理规则..."
	rm -f $DATA_PATH/user.bin
	rm -f $DATA_PATH/user.txt
	rulesnum=`nvram get adbybyrules_staticnum_x`
	[ -z "$rulesnum" ] && rulesnum=0
	if [ $adbyby_rules_x -eq 1 ]; then
		for i in $(seq 1 $rulesnum)
		do
			j=`expr $i - 1`
			rules_address=`nvram get adbybyrules_x$j`
			rules_road=`nvram get adbybyrules_road_x$j`
			[ -z "$rules_road" ] && rules_road=0
			if [ $rules_road -ne 0 ]; then
				logger -t "adbyby" "正在下载和合并第三方规则"
				curl -k -s -o /tmp/adbyby/user2.txt --connect-timeout 5 --retry 3 $rules_address
				grep -v '^!' /tmp/adbyby/user2.txt | grep -E '^(@@\||\||[[:alnum:]])' | sort -u | grep -v "^$" >> $DATA_PATH/user3adblocks.txt
				rm -f /tmp/adbyby/user2.txt
			fi
		done
		# 将user3adblocks.txt的非注释规则输出到user.txt
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
	[ -z "$num" ] && num=0
	if [ $adbyby_ip_x -eq 1 ]; then
		if [ $num -ne 0 ]; then
			logger -t "adbyby" "设置内网IP过滤控制"
			for i in $(seq 1 $num)
			do
				j=`expr $i - 1`
				ip=`nvram get adbybyip_ip_x$j`
				mode=`nvram get adbybyip_ip_road_x$j`
			[ -z "$mode" ] && mode=0
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
	
	# 参数验证，确保数值类型
	[ -z "$block_ios" ] && block_ios=0
	[ -z "$block_shortvideo" ] && block_shortvideo=0
	[ -z "$block_games" ] && block_games=0
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
address=/douyin.com/0.0.0.0
address=/douyinvod.com/0.0.0.0
address=/douyincdn.com/0.0.0.0
address=/tiktok.com/0.0.0.0
address=/tiktokcdn.com/0.0.0.0
address=/tiktokv.com/0.0.0.0
address=/bytedance.com/0.0.0.0
address=/toutiao.com/0.0.0.0
address=/amemv.com/0.0.0.0
address=/bytecdn.com/0.0.0.0
address=/ibytecdn.com/0.0.0.0
address=/sglog.com/0.0.0.0
address=/amemv.com/0.0.0.0

# 快手相关域名 (Kuaishou)
address=/kuaishou.com/0.0.0.0
address=/kwimgs.com/0.0.0.0
address=/ksad.com/0.0.0.0
address=/kuaishou.com.cn/0.0.0.0
address=/ksyungslb.com/0.0.0.0
address=/yximgs.com/0.0.0.0

# 小红书相关域名
address=/xiaohongshu.com/0.0.0.0
address=/xhslink.com/0.0.0.0
address=/xhsjpg.com/0.0.0.0
address=/xhsdsp.com/0.0.0.0
address=/redbook.com/0.0.0.0
address=/api.redbook.com/0.0.0.0

# 腾讯系短视频相关域名
address=/v.qq.com/0.0.0.0
address=/video.qq.com/0.0.0.0
address=/liveplay.qq.com/0.0.0.0
address=/video.gtimg.cn/0.0.0.0
address=/weishi.com/0.0.0.0
address=/weishi.qq.com/0.0.0.0
address=/video.api.weixin.qq.com/0.0.0.0
address=/mmbizapi.weixin.qq.com/0.0.0.0
address=/video.api.qq.com/0.0.0.0

# 西瓜视频
address=/ixigua.com/0.0.0.0
address=/toutiao.com/0.0.0.0
address=/snssdk.com/0.0.0.0

# 美拍
address=/meipai.com/0.0.0.0
address=/meitu.com/0.0.0.0
address=/meitudata.com/0.0.0.0

# 火山小视频
address=/huoshan.com/0.0.0.0
address=/huoshan.tv/0.0.0.0

# 梨视频
address=/pearvideo.com/0.0.0.0
address=/pearnode.com/0.0.0.0

EOF
	fi
	if [ $block_games -eq 1 ]; then
  		cat <<-EOF >/etc/storage/dnsmasq-adbyby.d/09-dnsmasq.games
# Popular Online Games Blocking (Valid domains only)
# 匹配域名 + 所有子域名 → address=/domain.com/0.0.0.0
# 加 ^ 表示精确匹配（不匹配子域名） → address=/^domain.com/0.0.0.0
# Tencent Games (应用连字符修复)
address=/pvp.qq.com/0.0.0.0
address=/game.qq.com/0.0.0.0
address=/down-update.qq.com/0.0.0.0
address=/update1.dlied.qq.com/0.0.0.0
address=/update5.dlied.qq.com/0.0.0.0
address=/oth.str.mdt.qq.com/0.0.0.0
address=/c.tdm.qq.com/0.0.0.0
address=/a.ssl.msdk.qq.com/0.0.0.0
address=/cloudctrl.gclud.qq.com/0.0.0.0
address=/masdk.3g.qq.com/0.0.0.0
address=/minigame.qq.com/0.0.0.0
address=/pubgmobile.qq.com/0.0.0.0
address=/sg-public-api.qq.com/0.0.0.0
address=/qqgame.qq.com/0.0.0.0
address=/wegame.com/0.0.0.0

#  NetEase Games
address=/minecraft.net/0.0.0.0
address=/session.minecraft.net/0.0.0.0
address=/game.163.com/0.0.0.0
address=/nie.163.com/0.0.0.0
address=/g79.update.netease.com/0.0.0.0
address=/g79.gdl.netease.com/0.0.0.0
address=/seadra.netease.com/0.0.0.0
address=/nie.netease.com/0.0.0.0
address=/superstar.pt.163.com/0.0.0.0
address=/x19.update.netease.com/0.0.0.0
address=/news-api.16163.com/0.0.0.0
address=/mgbsdk.matrix.netease.com/0.0.0.0
address=/api.k.163.com/0.0.0.0
address=/api.iplay.163.com/0.0.0.0
address=/gameyw.netease.com/0.0.0.0

# other
address=/steampowered.com/0.0.0.0
address=/steamcommunity.com/0.0.0.0
address=/pubg.com/0.0.0.0
address=/epicgames.com/0.0.0.0
address=/api.epicgames.com/0.0.0.0
address=/lewan.baidu.com/0.0.0.0
address=/game.hicloud.com/0.0.0.0
address=/biligame.com/0.0.0.0
address=/api.mihoyo.com/0.0.0.0
address=/api.miyoushe.com/0.0.0.0
address=/game.open.uc.cn/0.0.0.0

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
	
	add_rules
	
	# 启动adbyby程序
	cd $adbyby_dir && $PROG_PATH/adbyby &>/dev/null &
	sleep 2
	
	# 检查程序是否启动成功（优先检查PID文件）
	local startup_success=0
	if is_adbyby_running; then
		logger -t "adbyby" "AdByBy程序启动成功（PID文件检查）"
		startup_success=1
	elif pgrep -f "adbyby" > /dev/null; then
		logger -t "adbyby" "AdByBy程序启动成功（进程检查）"
		startup_success=1
	else
		logger -t "adbyby" "AdByBy程序启动失败"
		return 1
	fi
	
	if [ $startup_success -eq 1 ]; then
		# 初始化健康状态
		echo "0" > $HEALTH_STATE_FILE
		# 额外等待确保程序完全初始化
		sleep 1
	fi
	
	add_dns
	iptables-save | grep ADBYBY >/dev/null || \
	add_rule
	hosts_ads
	/sbin/restart_dhcpd
	#add_cron
	logger -t "adbyby" "AdByBy规则更新完成。"
}

# 清理域名，处理特殊字符（如连字符等）
cleanup_domain() {
	local domain="$1"
	# 移除域名前后的空白
	domain=$(echo "$domain" | xargs)
	# 确保域名格式正确，不包含特殊字符（除了合法的.-字符）
	domain=$(echo "$domain" | sed 's/[^a-zA-Z0-9.-]//g')
	# 处理连续的连字符和点号
	domain=$(echo "$domain" | sed 's/--*/-/g' | sed 's/\.\.*/\./g')
	# 移除开头和结尾的连字符和点号
	domain=$(echo "$domain" | sed 's/^[-\.]*//' | sed 's/[-\.]*$//')
	echo "$domain"
}

# 验证并修复有问题的域名规则
# 注意：连字符域名问题应该通过dnsmasq补丁修复，而不是在此处绕过
fix_problematic_domains() {
	local domain="$1"
	
	# 基本清理：只处理明显非法的情况
	case "$domain" in
		*--*)
			# 连续连字符 - 修复为单个连字符（这是合理的修复）
			echo "$domain" | sed 's/--*/-/g'
			;;
		-*)
			# 以连字符开头 - 移除开头连字符
			echo "$domain" | sed 's/^-*//'
			;;
		*-)
			# 以连字符结尾 - 移除结尾连字符
			echo "$domain" | sed 's/-*$//'
			;;
		*)
			# 正常域名，包括合法的连字符域名如 down-update.qq.com
			echo "$domain"
			;;
	esac
}

anti_ad(){
	anti_ad=`nvram get anti_ad`
	nvram set anti_ad_count=0
	# 参数验证，确保数值类型
	[ -z "$anti_ad" ] && anti_ad=0
	if [ "$anti_ad" = "1" ]; then
		# 清空之前的anti-AD规则文件
		rm -f /etc/storage/dnsmasq-adbyby.d/anti-ad-for-dnsmasq.conf
		
		# 检查anti-AD规则下载列表是否存在
		if [ -f "/etc/storage/adbyby_antiad.sh" ] && [ -s "/etc/storage/adbyby_antiad.sh" ]; then
			grep -v '^#' /etc/storage/adbyby_antiad.sh | grep -v "^$" > $adbyby_dir/antiad_list.txt
			if [ -s "$adbyby_dir/antiad_list.txt" ]; then
				local downloaded_files=0
				local total_rules=0
				
				# 智能处理混合规则：远程URL和本地规则
				local local_rules_count=0
				local remote_rules_count=0
				local local_rules_processed=0
				
				while IFS= read -r rule_line; do
					# 跳过空行和注释
					[ -z "$rule_line" ] || [ "${rule_line#\#}" != "$rule_line" ] && continue
					
					# 检查规则类型并分别处理
					case "$rule_line" in
						http://*|https://*)
							# 远程规则：下载处理
							logger -t "adbyby" "正在下载远程规则: $rule_line"
							local tempfile="/tmp/antiad_$(date +%s)_$downloaded_files.conf"
							
							if curl -k -s -o "$tempfile" --connect-timeout 5 --retry 3 "$rule_line"; then
								if [ -f "$tempfile" ] && [ -s "$tempfile" ]; then
									logger -t "adbyby" "远程规则下载成功: $rule_line"
									# 追加到规则文件（跳过可能的文件头注释）
									grep -v '^#!' "$tempfile" >> /etc/storage/dnsmasq-adbyby.d/anti-ad-for-dnsmasq.conf 2>/dev/null
									downloaded_files=$((downloaded_files + 1))
									remote_rules_count=$((remote_rules_count + 1))
									
									# 统计当前文件的规则数量
									local file_rules=$(grep -c -v '^[[:space:]]*#\|^$' "$tempfile" 2>/dev/null || echo 0)
									total_rules=$((total_rules + file_rules))
								else
									logger -t "adbyby" "远程规则文件为空: $rule_line"
								fi
							else
								logger -t "adbyby" "远程规则下载失败: $rule_line"
							fi
							
							# 清理临时文件
							rm -f "$tempfile"
							;;
						address=/*|server=/*)
							# 本地dnsmasq规则：直接写入
							echo "$rule_line" >> /etc/storage/dnsmasq-adbyby.d/anti-ad-for-dnsmasq.conf
							local_rules_count=$((local_rules_count + 1))
							local_rules_processed=$((local_rules_processed + 1))
							total_rules=$((total_rules + 1))
							logger -t "adbyby" "添加本地规则: ${rule_line}"
							;;
						*)
							# 检测是否为hosts格式规则（IP + 域名）
							if echo "$rule_line" | grep -q "^[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*[[:space:]]*[a-zA-Z0-9.-]"; then
								# hosts格式规则：转换为dnsmasq格式
								local domain=$(echo "$rule_line" | awk '{print $2}')
								local ip=$(echo "$rule_line" | awk '{print $1}')
								if [ -n "$domain" ] && [ -n "$ip" ]; then
									# 清理域名，处理特殊字符
									domain=$(cleanup_domain "$domain")
									# 基本修复（主要是明显的非法格式）
									domain=$(fix_problematic_domains "$domain")
									
									echo "address=/$domain/$ip" >> /etc/storage/dnsmasq-adbyby.d/anti-ad-for-dnsmasq.conf
									local_rules_count=$((local_rules_count + 1))
									local_rules_processed=$((local_rules_processed + 1))
									total_rules=$((total_rules + 1))
									logger -t "adbyby" "转换hosts规则: $domain -> $ip"
								fi
							# 检测是否为其他dnsmasq规则格式
							elif echo "$rule_line" | grep -q "^[a-z]*="; then
								# 对于address和server规则，需要清理域名
								if echo "$rule_line" | grep -q "^address=" || echo "$rule_line" | grep -q "^server="; then
									# 提取域名部分并清理
									local domain=$(echo "$rule_line" | sed -n 's/.*=\/*\([^/]*\).*/\1/p')
									if [ -n "$domain" ]; then
										domain=$(cleanup_domain "$domain")
										# 重新构建规则
										if echo "$rule_line" | grep -q "^address="; then
											local ip=$(echo "$rule_line" | sed -n 's/.*\/\([^/]*\)$/\1/p')
											rule_line="address=/$domain/$ip"
										elif echo "$rule_line" | grep -q "^server="; then
											local target=$(echo "$rule_line" | sed -n 's/.*\/\([^/]*\)$/\1/p')
											rule_line="server=/$domain/$target"
										fi
									fi
								fi
								echo "$rule_line" >> /etc/storage/dnsmasq-adbyby.d/anti-ad-for-dnsmasq.conf
								local_rules_count=$((local_rules_count + 1))
								local_rules_processed=$((local_rules_processed + 1))
								total_rules=$((total_rules + 1))
								logger -t "adbyby" "添加dnsmasq规则: $rule_line"
							else
								logger -t "adbyby" "跳过无法识别的规则格式: $rule_line"
							fi
							;;
						*)
							# 其他格式：尝试作为dnsmasq规则直接添加
							if echo "$rule_line" | grep -q "^[a-z]*="; then
								echo "$rule_line" >> /etc/storage/dnsmasq-adbyby.d/anti-ad-for-dnsmasq.conf
								local_rules_count=$((local_rules_count + 1))
								local_rules_processed=$((local_rules_processed + 1))
								total_rules=$((total_rules + 1))
								logger -t "adbyby" "添加dnsmasq规则: $rule_line"
							else
								logger -t "adbyby" "跳过无法识别的规则格式: $rule_line"
							fi
							;;
					esac
				done < $adbyby_dir/antiad_list.txt
				
				# 处理合并后的规则文件
				if [ $downloaded_files -gt 0 ] && [ -f "/etc/storage/dnsmasq-adbyby.d/anti-ad-for-dnsmasq.conf" ]; then
					# 去重处理
					sort /etc/storage/dnsmasq-adbyby.d/anti-ad-for-dnsmasq.conf | uniq > /tmp/anti_ad_sorted.conf
					mv /tmp/anti_ad_sorted.conf /etc/storage/dnsmasq-adbyby.d/anti-ad-for-dnsmasq.conf
					
					# 统计最终规则数量
					local final_rules=$(grep -c -v '^[[:space:]]*#\|^$' /etc/storage/dnsmasq-adbyby.d/anti-ad-for-dnsmasq.conf 2>/dev/null || echo 0)
					nvram set anti_ad_count=$final_rules
					logger -t "adbyby" "anti-AD规则处理完成: 远程规则$remote_rules_count个，本地规则$local_rules_count个，总计$final_rules条规则（去重后）"
				else
					logger -t "adbyby" "anti-AD规则处理失败：没有成功下载任何文件"
				fi
			else
				logger -t "adbyby" "anti-AD规则下载列表为空，跳过处理"
			fi
		else
			logger -t "adbyby" "anti-AD规则下载列表文件不存在，请先配置规则列表"
		fi

	fi
}

hosts_ads(){
	adbyby_hosts=`nvram get hosts_ad`
	nvram set adbyby_hostsad=0
	# 参数验证，确保数值类型
	[ -z "$adbyby_hosts" ] && adbyby_hosts=0
	if [ "$adbyby_hosts" = "1" ]; then
		rm -rf $DATA_PATH/hosts
		if [ -f "/etc/storage/adbyby_host.sh" ] && [ -s "/etc/storage/adbyby_host.sh" ]; then
			grep -v '^#' /etc/storage/adbyby_host.sh | grep -v "^$" > $DATA_PATH/hostlist.txt
			if [ -s "$DATA_PATH/hostlist.txt" ]; then
				for ip in `cat $DATA_PATH/hostlist.txt`
				do
					logger -t "adbyby" "正在下载: $ip"
					curl -k -s -o /tmp/host.txt --connect-timeout 5 --retry 3 $ip
					if [ ! -f "/tmp/host.txt" ]; then
						logger -t "adbyby" "$ip 下载失败！"
					else
						logger -t "adbyby" "hosts下载成功,处理中..."
						grep -v '^#' /tmp/host.txt | grep -v "^$" >> $DATA_PATH/hosts
					fi
				done
				rm -f /tmp/host.txt
				logger -t "adbyby" "正在对hosts文件进行去重处理."
				if [ -f "$DATA_PATH/hosts" ]; then
					sort $DATA_PATH/hosts | uniq > $DATA_PATH/hosts.tmp && mv $DATA_PATH/hosts.tmp $DATA_PATH/hosts
					nvram set adbyby_hostsad=`grep -v '^!' $DATA_PATH/hosts | wc -l`
					sed -i '/hosts/d' /etc/storage/dnsmasq/dnsmasq.conf
					cat >> /etc/storage/dnsmasq/dnsmasq.conf <<-EOF
			addn-hosts=$DATA_PATH/hosts
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
		rm -f $DATA_PATH/hosts
	fi
	rm -f $DATA_PATH/hostlist.txt
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
124.232.160.178

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
# AdByBy Hosts下载列表配置文件

# 常见广告过滤hosts
https://gitee.com/tekintian/adt-rules/raw/master/dnsmasq/ads_hosts.txt

EOF
		chmod 755 "$adbyby_host"
	fi

	adbyby_antiad="/etc/storage/adbyby_antiad.sh"
	if [ ! -f "$adbyby_antiad" ] || [ ! -s "$adbyby_antiad" ] ; then
		# 兼容旧版本：如果没有配置新列表，但存在旧配置，则使用旧配置
		local anti_ad_link="$(nvram get anti_ad_link)"
		
		# 如果系统默认配置为空，则使用默认链接
		[ -z "$anti_ad_link" ] && anti_ad_link="https://gitee.com/tekintian/adt-rules/raw/master/dnsmasq/anti-ad.conf"
	
		cat > "$adbyby_antiad" <<-EOF
# AdByBy anti-AD规则混合配置文件
# 支持多种规则格式混合使用：
# 
# 1. 远程规则（http/https开头）：自动下载并合并
#   https://gitee.com/tekintian/adt-rules/raw/master/dnsmasq/games.conf
#
# 2. 本地dnsmasq规则（address=/开头）：直接使用，无需下载
#    address=/ads.com/0.0.0.0
#    address=/tracker.example.com/
#    server=/ads.google.com/#
#
# 3. hosts格式规则（IP+域名）：自动转换为dnsmasq格式
#    0.0.0.0 ads.com
#    127.0.0.1 tracker.example.com
#
# 4. 其他dnsmasq规则：cache-stop、rebind-domain等
#    cache-stop=ads.google.com
#    rebind-domain-ok=/example.com/
#
# 所有规则会自动合并、去重后生成anti-ad-for-dnsmasq.conf

# Adbyby项目默认dnsmasq规则源
$anti_ad_link

# 游戏相关规则
# https://gitee.com/tekintian/adt-rules/raw/master/dnsmasq/games.conf
# 电商平台广告规则
# https://gitee.com/tekintian/adt-rules/raw/master/dnsmasq/shop.conf

EOF
		chmod 755 "$adbyby_antiad"
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
health_check)
health_check_adbyby
;;
*)
echo "Usage: $0 {start|stop|A|C|D|E|F|G|debug|init|health_check}"
echo "  start           - 启动AdByBy服务"
echo "  stop            - 停止AdByBy服务"
echo "  A               - 更新规则"
echo "  C               - 添加防火墙规则"
echo "  D               - 添加DNS规则"
echo "  E               - 添加脚本文件"
echo "  F               - 更新hosts文件"
echo "  G               - 更新规则并重启"
echo "  debug           - 显示调试信息"
echo "  init            - 初始化环境"
echo "  health_check    - 执行智能健康检查（自适应频率）"
echo ""
echo "注意：anti-AD规则现在支持多源配置，请通过Web界面管理规则列表"
;;
esac
