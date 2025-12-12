#!/bin/sh
# AdByBy-Open 安装脚本

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TARGET_DIR="/usr/share/adbyby"
BACKUP_DIR="/tmp/adbyby_backup_$(date +%Y%m%d_%H%M%S)"

echo "=== AdByBy-Open 安装程序 ==="
echo "源码目录: $SCRIPT_DIR"
echo "目标目录: $TARGET_DIR"
echo "备份目录: $BACKUP_DIR"

# 检查权限
if [ "$(id -u)" != "0" ]; then
    echo "错误: 需要root权限运行此脚本"
    echo "请使用: sudo $0"
    exit 1
fi

# 检查源文件
if [ ! -f "$SCRIPT_DIR/adbyby" ]; then
    echo "错误: 找不到编译后的adbyby程序"
    echo "请先运行: make"
    exit 1
fi

# 创建备份目录
mkdir -p "$BACKUP_DIR"

# 备份原有文件
echo "正在备份原有文件..."
if [ -f "$TARGET_DIR/adbyby" ]; then
    cp "$TARGET_DIR/adbyby" "$BACKUP_DIR/adbyby"
    echo "已备份: $TARGET_DIR/adbyby -> $BACKUP_DIR/adbyby"
fi

# 备份配置文件
for file in rules.txt user.txt lazy.txt video.txt; do
    if [ -f "$TARGET_DIR/data/$file" ]; then
        cp "$TARGET_DIR/data/$file" "$BACKUP_DIR/$file"
        echo "已备份: $TARGET_DIR/data/$file -> $BACKUP_DIR/$file"
    fi
done

# 安装新程序
echo "正在安装新程序..."
cp "$SCRIPT_DIR/adbyby" "$TARGET_DIR/adbyby"
chmod +x "$TARGET_DIR/adbyby"

# 创建默认规则文件（如果不存在）
mkdir -p "$TARGET_DIR/data"

if [ ! -f "$TARGET_DIR/data/rules.txt" ]; then
    cat > "$TARGET_DIR/data/rules.txt" << 'EOF'
# AdByBy-Open Rules File
# Format: pattern|type|description
# Types: 0=simple, 1=regex, 2=domain, 3=url, 4=wildcard

# 内置广告域名（程序会自动加载）
# 以下是自定义规则示例

# 常见广告域名
doubleclick.net|2|Google DoubleClick
googleadservices.com|2|Google Ads
googlesyndication.com|2|Google Syndication
google-analytics.com|2|Google Analytics
googletagmanager.com|2|Google Tag Manager

# 社交媒体跟踪
facebook.com/tr|0|Facebook Tracking
connect.facebook.net|2|Facebook Connect

# 其他广告网络
amazon-adsystem.com|2|Amazon Ads
taboola.com|2|Taboola
outbrain.com|2|Outbrain

# 通用广告路径模式
*/ad/*|4|广告路径
*/ads/*|4|广告路径
*/advertisement/*|4|广告页面
*/banner/*|4|横幅广告
*/popup/*|4|弹窗广告

# 跟踪和分析
*analytics*|0|分析服务
*tracking*|0|跟踪服务
*beacon*|0|信标跟踪
*pixel*|0|像素跟踪
EOF
    echo "已创建默认规则文件: $TARGET_DIR/data/rules.txt"
fi

# 创建用户自定义规则文件（如果不存在）
if [ ! -f "$TARGET_DIR/data/user.txt" ]; then
    cat > "$TARGET_DIR/data/user.txt" << 'EOF'
# 用户自定义规则
# 格式: pattern|type|description
# 在这里添加您自己的过滤规则
EOF
    echo "已创建用户规则文件: $TARGET_DIR/data/user.txt"
fi

# 测试安装
echo "正在测试安装..."
if "$TARGET_DIR/adbyby" -h > /dev/null 2>&1; then
    echo "✓ 程序运行正常"
else
    echo "✗ 程序运行异常"
    echo "正在恢复备份..."
    if [ -f "$BACKUP_DIR/adbyby" ]; then
        cp "$BACKUP_DIR/adbyby" "$TARGET_DIR/adbyby"
        echo "已恢复原有程序"
    fi
    exit 1
fi

# 显示规则统计
echo ""
echo "=== 规则统计 ==="
"$TARGET_DIR/adbyby" -s 2>/dev/null | grep -A 5 "Rule Statistics"

echo ""
echo "=== 安装完成 ==="
echo "新程序已安装到: $TARGET_DIR/adbyby"
echo "备份文件位置: $BACKUP_DIR"
echo ""
echo "使用方法:"
echo "  启动服务: $TARGET_DIR/adbyby"
echo "  调试模式: $TARGET_DIR/adbyby -d --no-daemon"
echo "  查看帮助: $TARGET_DIR/adbyby -h"
echo "  查看统计: $TARGET_DIR/adbyby -s"
echo ""
echo "配置文件位置:"
echo "  规则文件: $TARGET_DIR/data/rules.txt"
echo "  用户规则: $TARGET_DIR/data/user.txt"
echo ""
echo "注意事项:"
echo "1. 程序默认监听8118端口"
echo "2. 可以通过 -p 参数指定其他端口"
echo "3. 建议定期更新规则文件以获得最佳过滤效果"