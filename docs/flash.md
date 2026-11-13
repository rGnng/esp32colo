# ColoPLAY 刷机流程

当前仓库里的 `firmware/` 是一个最小的 ESP32-S3 固件：

- 连接 `OnIoT` WiFi（凭据在 `firmware/include/secrets.h`）
- 在 `http://coloplay.local/` 提供状态页
- `http://coloplay.local/api/status` 提供 JSON
- 支持 ArduinoOTA 无线升级
- 读取 GPIO0 / GPIO15 / GPIO46 三个按键状态
- **不初始化屏幕、音频等未确认外设**，适合作为第一次刷入的连通性测试

## 0. 先备份出厂固件

必须先把 16 MB 全片读出来，确认可恢复后再写。

```bash
./scripts/backup_factory.sh
# 或自动等待下载模式：
.venv/bin/python scripts/wait_and_backup.py
```

备份文件在 `backups/coloplay-factory-*.bin`，同时生成 SHA-256。

恢复出厂固件：

```bash
esptool --port /dev/cu.usbmodemXXXX --baud 460800 write-flash 0 backups/coloplay-factory-XXXX.bin
```

> 如果备份读不出来，或者安全信息显示 Secure Boot / Flash Encryption 已启用，
> 不要继续刷写；先停下来确认恢复路径。

## 1. 进入 ROM 下载模式

1. 拔掉 USB；如果设备有内部电池且拔电后仍然亮屏，先断开电池。
2. 按住电源板 **REC_KEY**，同时按下中间 **旋钮**。
3. 保持按住，插回 USB。
4. 约 1 秒后松开。

成功后电脑会出现 Espressif USB-JTAG 设备：

```text
USB VID:PID=303A:1001
```

对应的串口通常是 `/dev/cu.usbmodem*`。可以用以下命令确认：

```bash
python3 -m serial.tools.list_ports -v | grep -A2 303A:1001
```

## 2. 编译

```bash
./scripts/build_firmware.sh
```

脚本会编译并生成一个合并镜像 `firmware/firmware-merged.bin`（包含 bootloader、
分区表、OTA 数据和应用程序）。

## 3. 刷写

### 方式 A：PlatformIO（推荐）

```bash
cd firmware
pio run -t upload --upload-port /dev/cu.usbmodemXXXX
```

### 方式 B：写合并镜像

```bash
PORT=/dev/cu.usbmodemXXXX
esptool --port "$PORT" --baud 460800 write-flash 0x0 firmware/firmware-merged.bin
```

> ESP32-S3 的 bootloader 偏移是 `0x0`，不是 ESP32 的 `0x1000`。
> 当前工程按 N16R8（16 MB flash）和 `default_16MB.csv` 分区表构建；
> 如果备份检测到的容量不是 16 MB，先改 `firmware/platformio.ini`。

## 4. 验证

刷完后设备会自动重启，串口日志应出现：

```text
ColoPLAY bring-up firmware 0.1.0-bringup
[chip] ESP32-S3 ...
[wifi] connected: ip=... rssi=... mac=...
[http] server listening on port 80
[ota] ArduinoOTA ready
```

然后：

```bash
curl http://coloplay.local/api/status
```

或直接在浏览器打开 `http://coloplay.local/`。

如果 mDNS 不生效，用路由器后台查设备 IP，访问 `http://<IP>/`。

## 5. 后续无线升级

ArduinoOTA 密码在 `firmware/include/secrets.h` 的 `OTA_PASSWORD`。
在同一 WiFi 下可以用 PlatformIO 的 `upload_protocol = espota` 或 ArduinoOTA 工具升级，
不需要再按按钮。

## 6. 已知限制

- 屏幕、音频、按键灯、旋钮都还没有驱动；这是刻意的最小固件。
- 屏幕控制器型号尚未从原理图确认；ColoX 文档说明是 240×240 圆屏，
  常见候选为 GC9A01，需从出厂固件或屏排线确认。
- 如果设备启用了 Secure Boot，自定义固件无法启动，只能恢复出厂备份。
- WiFi 凭据会编译进固件；不要把 `firmware/include/secrets.h` 提交到公开仓库
  （已在 `.gitignore` 中忽略）。
