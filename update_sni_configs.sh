#!/bin/bash

echo "=== 批量更新SNI模块内核配置 ==="

# 查找所有需要更新的板级配置文件
config_files=$(find /Volumes/csdisk/padavan/trunk/configs/boards -name "kernel-4.4.x.config")

updated_count=0
total_count=0

for config in $config_files; do
    ((total_count++))
    
    # 检查是否有string配置但没有sni配置
    if grep -q "CONFIG_NETFILTER_XT_MATCH_STRING=y" "$config" && ! grep -q "CONFIG_NETFILTER_XT_MATCH_SNI=" "$config"; then
        echo "正在更新: $config"
        
        # 使用grep找到string配置的行号，然后在那行后插入SNI配置
        sed -i '' '/CONFIG_NETFILTER_XT_MATCH_STRING=y/a\
CONFIG_NETFILTER_XT_MATCH_SNI=y' "$config"
        
        ((updated_count++))
        echo "✅ 已更新"
    elif grep -q "CONFIG_NETFILTER_XT_MATCH_SNI=" "$config"; then
        echo "⚠️  跳过（已存在SNI配置）: $config"
    else
        echo "❌ 跳过（无STRING配置）: $config"
    fi
done

echo ""
echo "📋 更新总结"
echo "================================"
echo "总配置文件数: $total_count"
echo "已更新文件数: $updated_count"
echo "更新状态: $([ $updated_count -gt 0 ] && echo "✅ 成功" || echo "⚠️  无需更新")"
echo "================================"

# 验证更新结果
echo ""
echo "🔍 验证更新结果..."
sni_configs=$(grep -l "CONFIG_NETFILTER_XT_MATCH_SNI=y" /Volumes/csdisk/padavan/trunk/configs/boards/*/kernel-4.4.x.config | wc -l)
echo "包含SNI配置的文件数: $sni_configs/$total_count"

if [ $sni_configs -eq $total_count ]; then
    echo "✅ 所有配置文件都已包含SNI模块配置"
else
    echo "⚠️  仍有 $((total_count - sni_configs)) 个文件未包含SNI配置"
fi

echo ""
echo "=== 更新完成 ==="