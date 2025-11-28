#!/bin/sh
logger -t "adbyby" "后台更新Adblock Plus Host List,60s~120s完成后会自动刷新"
rm -f /tmp/dnsmasq.adblock

wget --no-check-certificate https://gitee.com/tekintian/adt-rules/raw/master/adbyby/dnsmasq.adblock -O /tmp/dnsmasq.adblock
if [ -s "/tmp/dnsmasq.adblock" ];then
	#sed -i '/youku.com/d' /tmp/dnsmasq.adblock
	#if ( ! cmp -s /tmp/dnsmasq.adblock /tmp/adbyby/dnsmasq.adblock );then
		mv /tmp/dnsmasq.adblock /tmp/adbyby/dnsmasq.adblock	
	#fi	
fi
#sh /tmp/adbyby/adupdate.sh
sleep 10 && /sbin/restart_dhcpd
logger -t "adbyby" "Adblock Plus Host List已更新完成"
nvram set adbyby_adb=`cat /tmp/adbyby/dnsmasq.adblock | wc -l`
