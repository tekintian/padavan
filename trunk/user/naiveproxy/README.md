# NaiveProxy for Padavan

NaiveProxy是一个支持SNI（Server Name Indication）的HTTP/HTTPS代理工具。

## 功能特点

- 支持SNI扩展
- SOCKS5代理
- TLS 1.3支持
- 自动padding防检测
- 高性能并发连接

## 配置方法

1. 编辑 `/etc/naiveproxy/naiveproxy.json` 文件
2. 修改代理服务器地址和认证信息
3. 重启naiveproxy服务

## 使用命令

```bash
# 启动服务
/etc/rc.d/S99naiveproxy start

# 停止服务
/etc/rc.d/S99naiveproxy stop

# 重启服务
/etc/rc.d/S99naiveproxy restart

# 查看状态
ps | grep naive
```

## 测试SNI功能

```bash
# 通过SOCKS5代理访问网站
curl -x socks5://127.0.0.1:1080 https://www.example.com

# 检查naiveproxy日志
logread | grep naiveproxy
```

## 注意事项

- 确保代理服务器支持HTTPS
- 配置文件中包含敏感信息，注意权限设置
- 防火墙需要开放1080端口（或你配置的端口）