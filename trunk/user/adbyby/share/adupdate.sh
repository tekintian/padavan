#!/bin/sh
logger -t "adbyby" "更新Adbyby规则"
rm -f /tmp/adbyby/data/*.bak

touch /tmp/local-md5.json && md5sum /tmp/adbyby/data/lazy.txt /tmp/adbyby/data/video.txt > /tmp/local-md5.json
touch /tmp/md5.json && wget --no-check-certificate -t 1 -T 10 -O /tmp/md5.json https://gitee.com/tekintian/adt-rules/raw/master/adbyby/md5.json

lazy_local=$(grep 'lazy' /tmp/local-md5.json | awk -F' ' '{print $1}')
video_local=$(grep 'video' /tmp/local-md5.json | awk -F' ' '{print $1}')  
lazy_online=$(sed  's/":"/\n/g' /tmp/md5.json  |  sed  's/","/\n/g' | sed -n '2p')
video_online=$(sed  's/":"/\n/g' /tmp/md5.json  |  sed  's/","/\n/g' | sed -n '4p')

if [ "$lazy_online"x != "$lazy_local"x -o "$video_online"x != "$video_local"x ]; then
    echo "MD5 not match! Need update!"
    touch /tmp/lazy.txt && wget --no-check-certificate -t 1 -T 10 -O /tmp/lazy.txt https://gitee.com/tekintian/adt-rules/raw/master/adbyby/lazy.txt
    touch /tmp/video.txt && wget --no-check-certificate -t 1 -T 10 -O /tmp/video.txt https://gitee.com/tekintian/adt-rules/raw/master/adbyby/video.txt
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
fi

rm -f /tmp/lazy.txt /tmp/video.txt /tmp/local-md5.json /tmp/md5.json
logger -t "adbyby" "Adbyby规则更新完成"