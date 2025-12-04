# Padavan 编译环境

## 构建镜像
~~~sh
CURRENT_DIR=$(cd "$(dirname "$0")" && pwd)



docker build --build-arg APT_MIRROR_HOST=mirrors.tuna.tsinghua.edu.cn -t tekintian/padavan-compiler:4.4.198 .
~~~


## 运行容器
~~~sh
docker run -itd --name padavan -v /Volumes/csdisk/padavan:/Volumes/csdisk/padavan padavan-compiler:4.4.198
~~~

## 编译器路径
/usr/local/mipsel-toolchain-4.4.x

## 编译
~~~sh
cd /padavan/trunk
./build_padavan.sh K2P
~~~
