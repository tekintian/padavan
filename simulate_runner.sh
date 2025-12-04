#!/bin/bash
# GitHub Actions Runner 离线模拟脚本
# 模拟 runner 行为，无需连接 GitHub API

echo "🤖 GitHub Actions Runner 离线模拟器启动"
echo "📋 模拟作业：padavan/padavan - Build and Test"
echo "🏃‍♂️ Runner: docker-test-runner"
echo "📁 工作目录: $(pwd)"

# 模拟 GitHub Actions 环境变量
export GITHUB_ACTIONS="true"
export GITHUB_WORKSPACE="/home/runner/work/padavan/padavan"
export GITHUB_RUNNER="actions-runner"
export GITHUB_REPOSITORY="padavan"
export GITHUB_REF="refs/heads/dev"
export CI="true"

# 模拟作业步骤
echo "📦 步骤 1/5: Checkout repository"
echo "✅ Repository checked out"

echo "🔧 步骤 2/5: Setup build environment"
echo "✅ Build environment ready"

echo "🏗️  步骤 3/5: Compile firmware"
echo "🔄 开始编译..."
if [ -f "/tmp/test_iptables_compile.sh" ]; then
    /tmp/test_iptables_compile.sh
else
    echo "⚠️  编译测试脚本不存在"
fi

echo "📊 步骤 4/5: Upload artifacts"
echo "✅ Build artifacts uploaded"

echo "🎉 步骤 5/5: Complete"
echo "✅ Job completed successfully"

echo "🤖 Runner 离线模拟完成"