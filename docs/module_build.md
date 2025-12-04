
保存异常数据解决方案：

## 1. 保存异常数据到文件

首先，我们需要修改代码来保存无法识别的数据包。这里是一个实现方案：

```c:/Volumes/csdisk/padavan/trunk/linux-4.4.x/net/netfilter/xt_sni_filter.c
// 在文件开头添加必要的头文件
#include <linux/fs.h>
#include <linux/uaccess.h>

// 添加全局变量来控制是否保存异常数据
static bool save_failed_packets = false;
module_param(save_failed_packets, bool, 0644);
MODULE_PARM_DESC(save_failed_packets, "Save failed packets to file for debugging");

// 添加保存数据包到文件的函数
static void save_packet_data(const u_int8_t *data, size_t len, const char *reason)
{
    struct file *filp;
    loff_t pos = 0;
    char filename[64];
    char header[128];
    int header_len;
    int ret;

    if (!save_failed_packets)
        return;

    // 创建唯一文件名
    snprintf(filename, sizeof(filename), "/tmp/sni_failed_packet_%lu.dat", (unsigned long)get_seconds());
    
    filp = filp_open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (IS_ERR(filp)) {
        DEBUGP("Failed to open file %s for writing failed packet\n", filename);
        return;
    }

    // 写入头部信息
    header_len = snprintf(header, sizeof(header), 
                         "Packet saved at: %lu\n"
                         "Reason: %s\n"
                         "Length: %zu\n"
                         "First 64 bytes (hex): ",
                         (unsigned long)get_seconds(), reason, len);
    
    ret = kernel_write(filp, header, header_len, &pos);
    if (ret < 0) {
        DEBUGP("Failed to write header to %s\n", filename);
        filp_close(filp, NULL);
        return;
    }
    pos += ret;

    // 写入数据的十六进制表示
    if (len > 0 && data) {
        char hex_buf[128];
        size_t i, write_len;
        size_t bytes_to_write = min(len, (size_t)64); // 只写前64字节
        
        for (i = 0; i < bytes_to_write; i++) {
            if (i % 16 == 0 && i > 0) {
                snprintf(hex_buf, sizeof(hex_buf), "\n");
                kernel_write(filp, hex_buf, strlen(hex_buf), &pos);
            }
            snprintf(hex_buf, sizeof(hex_buf), "%02x ", data[i]);
            kernel_write(filp, hex_buf, strlen(hex_buf), &pos);
        }
        snprintf(hex_buf, sizeof(hex_buf), "\n");
        kernel_write(filp, hex_buf, strlen(hex_buf), &pos);
    }

    filp_close(filp, NULL);
    DEBUGP("Saved failed packet to %s\n", filename);
}
```

然后在适当的位置调用这个函数：

```c:/Volumes/csdisk/padavan/trunk/linux-4.4.x/net/netfilter/xt_sni_filter.c
// 在is_tls_client_hello函数中添加调用
static bool is_tls_client_hello(const struct sk_buff *skb, u_int8_t protocol)
{
    // ... 原有代码 ...

    /* 检查TLS记录头 */
    if (skb_copy_bits(skb, payload_offset, &record_type, 1) != 0) {
        DEBUGP("Failed to copy record type\n");
        // 保存失败的数据包
        u_int8_t temp_byte;
        if (skb_copy_bits(skb, payload_offset, &temp_byte, 1) != 0) {
            save_packet_data(NULL, 0, "Failed to copy record type");
        }
        return false;
    }

    if (record_type != TLS_HANDSHAKE) {
        DEBUGP("Record type mismatch: expected %u, got %u\n", TLS_HANDSHAKE, record_type);
        // 保存失败的数据包
        u_int8_t first_bytes[16];
        if (skb_copy_bits(skb, payload_offset, first_bytes, min(16U, data_len)) == 0) {
            save_packet_data(first_bytes, min(16U, data_len), "Record type mismatch");
        }
        return false;
    }

    // ... 后续代码 ...
}
```

## 2. 重新编译模块的详细步骤

### 步骤1: 准备编译环境
```bash
# 进入内核源码目录
cd /Volumes/csdisk/padavan/trunk/linux-4.4.x

# 确保有正确的编译配置
make ARCH=mips CROSS_COMPILE=mipsel-linux-musl- prepare
```

### 步骤2: 编写Makefile（如果没有的话）
创建一个简单的Makefile：
```makefile
obj-m += xt_sni_filter.o

KDIR := /Volumes/csdisk/padavan/trunk/linux-4.4.x
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
```

### 步骤3: 编译模块
```bash
# 清理之前的编译结果
make clean

# 编译模块
make ARCH=mips CROSS_COMPILE=mipsel-linux-musl-

# 如果编译成功，会生成 xt_sni_filter.ko 文件
```

### 步骤4: 部署到路由器
```bash
# 1. 将编译好的模块复制到路由器
scp xt_sni_filter.ko root@router_ip:/tmp/

# 2. 在路由器上执行以下命令
ssh root@router_ip

# 卸载旧模块
rmmod xt_sni_filter

# 备份原模块（可选）
cp /lib/modules/4.4.198/kernel/net/netfilter/xt_sni_filter.ko /lib/modules/4.4.198/kernel/net/netfilter/xt_sni_filter.ko.backup

# 替换模块
cp /tmp/xt_sni_filter.ko /lib/modules/4.4.198/kernel/net/netfilter/xt_sni_filter.ko

# 加载新模块
insmod /lib/modules/4.4.198/kernel/net/netfilter/xt_sni_filter.ko

# 或者使用 modprobe
modprobe xt_sni_filter
```

### 步骤5: 启用调试和异常数据保存
```bash
# 启用调试模式
echo 1 > /sys/module/xt_sni_filter/parameters/enable_debug

# 启用异常数据保存
echo 1 > /sys/module/xt_sni_filter/parameters/save_failed_packets
```

### 步骤6: 监控和收集数据
```bash
# 查看内核日志
dmesg -w | grep SNI-FILTER

# 检查是否生成了异常数据文件
ls -la /tmp/sni_failed_packet_*.dat

# 查看异常数据文件内容
cat /tmp/sni_failed_packet_*.dat
```

## 3. 更简化的本地测试方法

如果您想在本地开发环境中测试而不需要交叉编译，可以这样做：

```bash
# 在本地Linux环境中测试（仅用于开发调试）
cd /Volumes/csdisk/padavan/trunk/linux-4.4.x/net/netfilter

# 创建简化版的测试Makefile
cat > test_Makefile << 'EOF'
obj-m += xt_sni_filter.o

# 使用当前系统的内核头文件
KDIR := /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
EOF

# 编译测试
make -f test_Makefile clean
make -f test_Makefile
```

## 4. 故障排除

如果编译过程中遇到问题：

1. **缺少头文件**：
   ```bash
   # 确保安装了内核头文件
   sudo apt-get install linux-headers-$(uname -r)
   ```

2. **交叉编译工具链问题**：
   ```bash
   # 确保交叉编译工具链已正确安装
   export PATH=/usr/local/mipsel-toolchain-4.4.x/bin:$PATH
   ```

3. **模块依赖问题**：
   ```bash
   # 清理并重新编译
   make clean
   make ARCH=mips CROSS_COMPILE=mipsel-linux-musl- clean
   make ARCH=mips CROSS_COMPILE=mipsel-linux-musl-
   ```

通过这种方式，您可以捕获和分析那些无法识别的数据包，从而更好地理解和解决问题。保存的文件将包含数据包的原始内容，便于后续分析和复现问题。
        