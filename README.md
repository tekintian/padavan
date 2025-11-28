# padavan 固件

This project is based on original rt-n56u with latest mtk 4.4.198 kernel, which is fetch from D-LINK GPL code.
 
## Padavan 固件版本功能模块列表与适用场景说明

本固件支持多种基于MT7621等平台的路由器设备，包括但不限于：K2(测试中)、K2P、K2P-USB、MI-R3G、R2100、XY-C1等多款路由器。

## K2P系列固件版本对比

### 1. K2P（标准版）

**适用场景**：
- 需要完整功能的普通家庭用户
- 对广告拦截、网络代理、网络工具等有全面需求的用户
- 不需要USB存储功能的K2P用户

**核心功能模块**：
- **广告管理**：集成adbyby plus+广告拦截
- **网络代理**：支持Shadowsocks、Trojan、simple-obfs等
- **DNS服务**：内置SmartDNS智能解析
- **安全通信**：完整的OpenSSH服务
- **网络工具**：包含curl、ttyd、htop、iperf3、mtr、msd_lite等
- **网络优化**：支持SQM智能队列管理和WireGuard VPN
- **安全认证**：支持EAP-PEAP企业级认证
- **高级功能**：IPSet、完整OpenSSL支持

### 2. K2P-NANO（精简版）

**适用场景**：
- 需要广告拦截但不需要代理工具的用户
- 对固件稳定性有较高要求的用户
- 希望平衡功能和性能的用户

**核心功能模块**：
- **广告管理**：保留adbyby plus+广告拦截功能
- **系统监控**：包含htop进程监控工具
- **网络测试**：保留iperf3、mtr等基础网络诊断工具
- **网络优化**：支持SQM和轻量级WireGuard
- **安全管理**：使用功能完整的OpenSSH
- **精简设计**：移除了Shadowsocks、Trojan等代理工具和SmartDNS

### 3. K2P-TINY（超精简版）

**适用场景**：
- 对固件大小要求极高的场景（如固件空间受限）
- 仅需要基本路由功能的用户
- 追求极致稳定性和性能的用户
- 作为二级路由或专用设备使用

**核心功能模块**：
- **轻量级设计**：移除几乎所有可选功能，最小化固件体积
- **SSH管理**：使用更轻量级的dropbear替代OpenSSH
- **网络加速**：保留SFE硬件加速功能
- **基础诊断**：保留htop、iperf3、mtr等核心网络工具
- **基础功能**：支持IPv6、IPSet、EAP-PEAP认证
- **轻量VPN**：支持WireGuard轻量级VPN协议
- **移除的功能**：无广告管理、无代理工具、无DNS增强服务

### 4. K2P-USB（增强版, 注意这个版本需要改造过支持USB的K2P才可用）

**适用场景**：
- 使用K2P-USB硬件版本的用户
- 需要USB存储功能的用户
- 需要文件共享和媒体服务器的用户
- 需要VPN服务器功能的用户

**核心功能模块**：
- **USB支持**：完整的USB功能和设备驱动支持
- **文件系统**：支持FAT/FAT32、exFAT、EXT2/3/4、FUSE等多种文件系统
- **存储服务**：包含SMB3.6文件共享服务器、FTP服务器
- **打印服务**：支持LPR打印守护进程、U2EC打印共享
- **网络工具**：丰富的网络工具集，包括tcpdump、parted等
- **VPN服务**：包含OpenVPN完整支持
- **代理工具**：支持Shadowsocks、Trojan、Xray等多种代理
- **专业诊断**：完整的网络诊断和维护工具


## Padavan路由器固件可选插件列表

根据README.md和配置文件，以下是Padavan固件支持的可选插件列表，按功能类别整理：

### 1. 网络代理与加速工具
- **Shadowsocks** - 代理工具，支持科学上网
- **Trojan** (~1.2M) - 代理协议工具
- **Xray** (~4.5M) - 代理工具
- **V2ray** - 代理工具
- **Sing-box** - 代理工具
- **Simple-obfs** - 代理混淆工具
- **Naiveproxy** - 基于Chrome浏览器的代理工具
- **Srelay** - 简易Socks5代理服务器
- **Redsocks** - 重定向TCP连接的工具

### 2. 广告拦截
- **Adbyby plus+** - 广告拦截工具
- **AdGuardHome** (~8M) - 广告拦截与DNS过滤
- **SmartDNS** - 智能DNS解析器，可减少DNS污染
- **DNS-Forwarder** - DNS转发工具

### 3. 网络工具与诊断
- **TCPDump** (~0.6MB) - 网络数据包捕获工具
- **IPerf3** - 网络性能测试工具
- **MTR** - 网络路由追踪与丢包分析工具
- **Socat** - 网络工具，可以在不同的协议之间转发数据
- **SQM-QoS** - 智能队列管理与服务质量控制
- **MSD_Lite** - 替代udpxy的IPTV工具
- **XUPNPd** (~0.3MB) - IPTV媒体服务器

### 4. 文件系统与存储
- **FAT/FAT32** (~0.1MB) - 文件系统支持
- **exFAT** (~0.12MB) - 文件系统支持
- **EXT2** (~0.1MB) - 文件系统支持
- **EXT3** (~0.2MB) - 文件系统支持
- **EXT4** (~0.4MB) - 文件系统支持
- **XFS** (~0.6MB) - 文件系统支持
- **FUSE** (~0.1MB) - 用户空间文件系统支持
- **ANTFS** - AVM NTFS驱动
- **NTFS-3G** (~0.4MB) - NTFS文件系统支持
- **Swap** (~0.05MB) - 交换分区支持
- **HDPARM** (~0.1MB) - 硬盘参数设置工具
- **Parted** (~0.3MB) - 分区工具

### 5. 远程访问与SSH
- **OpenSSH** (~1.0MB) - SSH服务（功能更完整）
- **Dropbear** (~0.3MB) - 轻量级SSH服务
- **TTYD** - 基于浏览器的终端工具
- **HTTPS** (~1.2MB) - 安全网页服务支持
- **SFTP-Server** (~0.06MB) - SFTP服务

### 6. VPN服务
- **OpenVPN** (~0.4MB) - VPN服务（需要IPv6支持）
- **StrongSwan** (~0.7MB) - IPsec VPN服务
- **WireGuard** (10K) - 轻量级VPN服务
- **SoftEtherVPN** - VPN服务器/客户端
- **ZeroTier** (~1.3M) - 虚拟局域网工具
- **FRP** - 内网穿透工具（客户端/服务端）

### 7. 服务器功能
- **SMB3.6** (~1.5MB) - 文件共享服务器
- **WINS** (~0.4MB) - WINS服务器
- **FTPD** (~0.2MB) - FTP服务器
- **Minidlna** (~1.6MB) - UPnP媒体服务器
- **Firefly** (~1.0MB) - iTunes媒体服务器
- **FFmpeg_NEW** (~0.1MB) - 新版FFmpeg，用于媒体服务器
- **VLMCSD** - KMS激活服务器
- **DDNSTO** (~0.5M) - 内网穿透工具

### 8. 下载工具
- **Transmission** (~1.5MB) - BT下载客户端
- **Aria2** (~3.5MB) - 多协议下载工具
- **Aria2 WEB Control** (~0.7MB) - Aria2的Web控制界面

### 9. USB设备支持
- **USB Support** - 基础USB支持
- **UVC** (~0.2MB) - USB摄像头支持
- **USB-HID** (~0.2MB) - USB人机接口设备支持
- **USB-Serial** (~0.03MB) - USB串口设备支持
- **USB-Audio** (~0.46MB) - USB音频设备支持
- **LPRD** (~0.12MB) - 打印守护进程
- **U2EC** (~0.05MB) - USB转以太网打印守护进程

### 10. 校园网认证
- **SCUTCLIENT** - 华南理工大学客户端
- **GDUT-DRCOM** - 广东工业大学客户端
- **Dogcom** - 校园网客户端
- **Minieap** - 校园网客户端
- **NJIT-Client** - 南京工程学院客户端
- **MENTOHUST** - 华中科技大学客户端

### 11. 系统工具
- **CURL** - 网络请求工具
- **NANO** - 文本编辑器
- **HTOP** - 进程监控工具
- **LZRSZ** - 文件传输工具
- **DUMP1090** - 航空数据接收工具
- **RTL_SDR** - 软件定义无线电支持
- **ALDRIVER** (~3M) - 阿里驱动

### 12. 其他功能
- **IPv6 Support** - IPv6协议支持
- **XFRM** (~0.2MB) - IPsec模块
- **QoS** (~0.2MB) - 服务质量控制
- **IMQ** (~0.02MB) - 流量整形模块
- **IFB** (~0.03MB) - 流量整形模块
- **IPSet** (~0.4MB) - IP集合管理工具
- **OpenSSL_EC** (~0.1MB) - OpenSSL椭圆曲线支持
- **OpenSSL_EXE** (~0.4MB) - OpenSSL命令行工具
- **ALIDNS** - 阿里云动态DNS
- **OC** - CPU超频支持

### 如何启用/禁用插件
在 `trunk/configs/templates/K2P.config` 文件中，通过设置对应选项的值为 `y`（启用）或 `n`（禁用）来控制是否包含某个插件。例如：
```
CONFIG_FIRMWARE_INCLUDE_ADBYBY=y  # 启用Adbyby插件
CONFIG_FIRMWARE_INCLUDE_SHADOWSOCKS=n  # 禁用Shadowsocks插件
```

### 注意事项
- K2P 128M版本构建后的.trx文件最大支持15.68MB，超过此大小的固件无法刷入
- 启用插件会增加固件大小，需要根据设备内存和需求合理选择


## Features


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
