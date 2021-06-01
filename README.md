# 说明

进程管理工具，能够根据配置文件自动启动进程，通过udp发送json控制命令可以控制批量启动关闭进程

# 配置文件模板

见[caller_table.json](res/conf/caller_table.json)

# 协议

协议通过udp报文传输json控制命令

33496

```json
{
  "mode_name": "mode_com",
  "enable": true
}
```

模拟报文发送

```bash
echo "{\"mode_name\":\"mode_1\",\"enable\":true}" | nc -u 127.0.0.1 33446
```

tcp查看工具

```bash
sudo tcpdump -i lo -nn -vv -X udp port 33496
```