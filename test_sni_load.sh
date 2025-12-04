#!/bin/bash

echo "=== SNI模块加载测试 ==="

# 进入build目录
cd /padavan/trunk/build/iptables-1.8.7

# 创建必要的头文件
echo "📝 创建xtables-version.h..."
echo '#define XTABLES_VERSION "1.8.7"' > include/xtables-version.h

# 编译SNI扩展
echo "🔧 编译SNI扩展..."
cd extensions
gcc -shared -fPIC -I../include -I../include/xtables -I../../../linux-4.4.x/include/uapi -I../../../linux-4.4.x/include -o libxt_sni.so libxt_sni.c

if [ $? -eq 0 ]; then
    echo "✅ SNI扩展编译成功！"
else
    echo "❌ SNI扩展编译失败！"
    exit 1
fi

# 检查SNI扩展的基本信息
echo "📊 SNI扩展信息："
echo "文件大小: $(stat -c%s libxt_sni.so) bytes"
echo "依赖库: $(ldd libxt_sni.so 2>/dev/null || echo '无动态依赖')"

# 检查关键符号
echo "🔍 关键符号检查："
echo "_INIT符号: $(nm -D libxt_sni.so | grep _INIT || echo '未找到')"
echo "xtables相关符号: $(nm -D libxt_sni.so | grep xtables | wc -l) 个"

# 创建简单的测试程序来验证模块格式
echo "🧪 创建测试程序..."
cat > test_sni_format.c << 'EOF'
#include <stdio.h>
#include <dlfcn.h>
#include <xtables.h>

int main() {
    void *handle;
    void (*init_func)(void);
    
    printf("测试加载SNI扩展...\n");
    
    handle = dlopen("./libxt_sni.so", RTLD_LAZY);
    if (!handle) {
        printf("加载失败: %s\n", dlerror());
        return 1;
    }
    
    init_func = (void (*)())dlsym(handle, "_init");
    if (init_func) {
        printf("✅ 找到_init函数\n");
        // 不实际调用，因为可能需要特定的环境
    } else {
        printf("⚠️  未找到_init函数: %s\n", dlerror());
    }
    
    dlclose(handle);
    printf("✅ 基本加载测试通过\n");
    return 0;
}
EOF

gcc -o test_sni_format test_sni_format.c -ldl
if [ $? -eq 0 ]; then
    echo "🔬 运行格式测试..."
    ./test_sni_format
else
    echo "⚠️  无法编译测试程序"
fi

echo ""
echo "=== SNI模块测试完成 ==="