# padavan #

This project is based on original rt-n56u with latest mtk 4.4.198 kernel, which is fetch from D-LINK GPL code.

##### Enhancements in this repo

- commits has beed rewritten on top of [hanwckf/rt-n56u](https://github.com/hanwckf/rt-n56u) repo for better history tracking
- Optimized Makefiles and build scripts, added a toplevel Makefile
- Added ccache support, may save up to 50%+ build time
- Upgraded the toolchain and libc:
  - gcc 13.2.0
  - musl 1.2.4
 - OpenWrt style package Makefile
 - Enabled kernel cgroups support
 - Fixed K2P led label names
 - Replaced udpxy with msd_lite
 - Replaced Web Console with ttyd
 - Upgraded libs and user packages
 - And a lot of package related fixes
 - ...

## 可选插件列表 List of optional plugins
根据自己的需要在 trunk/configs/templates/K2P.config 这个配置文件里面 开启 y 或者关闭 n 插件或功能
如: CONFIG_FIRMWARE_INCLUDE_ADBYBY=y 表示安装Adbyby插件, 如果是 n 表示不安装这个插件;

注意: K2P 128M 版本构建后的.trx文件最大 15.68M 大于这个的固件无法刷入, breed web控制台直接提示错误!!

~~~txt
3proxy             hdparm             njit-client        sqm-qos
802.1x             htop               ntfs-3g            srelay
LPRng              httpd              nvram              strongswan
Makefile           igmpproxy          openssh            tcpdump
adbyby             inadyn             openvpn            transmission
adguardhome        infosvr            optware            trojan
aliddns            iperf3             p910nd             ttyd
aliyundrive-webdav iproute2           parted             u2ec
antfs              ipset              poptop             udpxy
aria2              ipt2socks          pppd               uqmi
iptables           pppoe              usb-modeswitch
busybox            lanauth            pppoe-relay        util-linux
libdisk            ralinkiappd        utils
chnroute           lldt               rc                 v2ray
comgt-0.32         lrzsz              redsocks           vlmcsd
coremark           lua51              resolveip          vsftpd
ddnsto             mentohust          rp-l2tp            wg-quick
dns-forwarder      microsocks         rpcbind            wing
dns2tcp            minidlna           samba36            wireguard
dnsmasq            minieap            scripts            wireguard-tools
dogcom             miniupnpd          scutclient         wireless-tools
dosfstools         msd_lite           shadowsocks        wpa_supplicant
dropbear           mtd-utils          shared             wsdd2
dump1090           mtd_write          simple-obfs        www
e2fsprogs          mtr                sing-box           xTun
ebtables           naiveproxy         skipdbv2           xl2tpd
firefly            nano               smartdns           xray
frp                networkmap         socat              xupnpd
gdut-drcom         nfs-utils          softethervpn       zerotier
~~~

# Features

- Based on 4.4.198 Linux kernel
- Support MT7621 based devices
- Support MT7615D/MT7615N/MT7915D wireless chips
- Support raeth and mt7621 hwnat with legency driver
- Support qca shortcut-fe
- Support IPv6 NAT based on netfilter
- Support WireGuard integrated in kernel
- Support fullcone NAT (by Chion82)
- Support LED&GPIO control via sysfs

# Supported devices

- CR660x
- JCG-Q20
- JCG-AC860M
- JCG-836PRO
- JCG-Y2
- DIR-878
- DIR-882
- K2P
- K2P-USB
- NETGEAR-BZV
- MR2600
- MI-4
- MI-R3G
- MI-R3P
- R2100
- XY-C1

# Compilation steps

- Install dependencies
  ```sh
  # Debian/Ubuntu
  sudo apt install unzip libtool-bin ccache curl cmake gperf gawk flex bison nano xxd \
      fakeroot kmod cpio bc zip git python3-docutils gettext automake autopoint \
      texinfo build-essential help2man pkg-config zlib1g-dev libgmp3-dev \
      libmpc-dev libmpfr-dev libncurses5-dev libltdl-dev wget libc-dev-bin
  ```
  **Optional:**
  - install [golang](https://go.dev/doc/install) for building go programs
    ```sh
    sudo rm -rf /usr/local/go
    curl -fsSL https://go.dev/dl/go1.20.10.linux-amd64.tar.gz | sudo tar -C /usr/local -xz
    echo "export PATH=\$PATH:/usr/local/go/bin" | sudo tee /etc/profile.d/go.sh
    source /etc/profile.d/go.sh
    go version
    ```
  - install [nodejs](https://nodejs.org/en/download) for building [AdGuardHome](trunk/user/adguardhome)
    ```sh
    sudo apt update
    sudo apt install -y ca-certificates curl gnupg
    sudo mkdir -p /etc/apt/keyrings
    curl -fsSL https://deb.nodesource.com/gpgkey/nodesource-repo.gpg.key | sudo gpg --dearmor -o /etc/apt/keyrings/nodesource.gpg
    echo "deb [signed-by=/etc/apt/keyrings/nodesource.gpg] https://deb.nodesource.com/node_18.x nodistro main" | sudo tee /etc/apt/sources.list.d/nodesource.list
    sudo apt update
    sudo apt install -y nodejs
    node -v
    ```
- Clone source code
  ```sh
  git clone https://github.com/tsl0922/padavan.git
  ```
- Modify template file and start compiling
  ```sh
  # (Optional) Modify template file
  # vi trunk/configs/templates/K2P.config

  # Start compiling with: make PRODUCT_NAME
  make K2P

  # To build firmware for other devices, clean the tree after previous build
  make clean
  ```

# Package Development

- Makefile examples
  - [Makefile project](trunk/libs/libpcre/Makefile) 
  - [CMake project](trunk/user/ttyd/Makefile)
- Compiling a single package (cd to `trunk` first)
  - build: `make libs/libpcre_only`
  - clean: `make libs/libpcre_clean`
  - romfs: `make libs/libpcre_romfs`

# Manuals

- Controlling GPIO and LEDs via sysfs
- How to use NAND RWFS partition
- How to use IPv6 NAT and fullcone NAT
- How to add new device support with device tree
