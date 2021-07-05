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
echo "{\"mode_name\":\"mode_1\",\"enable\":true}" | nc -u 127.0.0.1 33496
```

tcp查看工具

```bash
sudo tcpdump -i lo -nn -vv -X udp port 33496
```

systemd

Lastly, we’re going to see how to run a script with systemd. Similarly to init.d, we need to create a service descriptor – called a unit file – under /etc/systemd/system:

```ini
[Unit]
Description=Reboot message systemd service.

[Service]
Type=simple
ExecStart=/bin/bash /home/ec2-user/reboot_message.sh

[Install]
WantedBy=multi-user.target
```
The file is organized into different sections:

    Unit – contains general metadata, like a human-readable description
    Service – describes the process and daemonizing behavior, along with the command to start the service
    Install – enables the service to run at startup, using the folder specified in WantedBy to handle dependencies

To finish up, we need to set the file permissions to 644 and enable our service by using systemctl:

```
$ chmod 644 /etc/systemd/system/reboot_message.service
$ systemctl enable reboot_message.service
```

One thing to keep in mind is that although many major distributions support systemd, it’s not always available.