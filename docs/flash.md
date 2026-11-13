# ColoPLAY 刷机流程

当前可用的固件是 `firmware/` 下的最小 bring-up 应用：

- 连接 `OnIoT` WiFi（凭据保存在 NVS，首次启动从 `secrets.h` 播种）
- `http://coloplay.local/` 状态页，`/api/status` JSON
- ArduinoOTA 无线升级
- 读取 GPIO0 / GPIO15 / GPIO46 按键状态
- **不初始化屏幕、音频等外设**，用于先建立 WiFi + OTA 通道

## 关键事实

这台机器已经确认：

- ESP32-S3-WROOM-1-N16R8（16 MB flash，8 MB OPI PSRAM）
- 屏幕控制器 **GC9A01**（240×240 圆屏）
- 音频 codec **ES8311**
- Secure Boot / Flash Encryption 均未启用
- 原厂 bootloader 在 `0x0`，原厂分区表在 `0x10000`（不是常见的 `0x8000`）
- 应用槽：`ota_app0` @ `0x20000`，`ota_app1` @ `0x4a0000`

**保留原厂 bootloader**。Arduino 默认 bootloader 在这颗 flash
（Manufacturer `0x46`）上会反复触发 WDT 重启，而原厂 bootloader 可以正常启动
我们的应用。因此首次刷写只替换应用，不覆盖 `0x0` 和 `0x10000`。

原厂 bootloader 每次启动会打印：

```text
SHA-256 comparison failed:
...
Attempting to boot anyway...
```

这是原厂 bootloader 对自身镜像头的校验警告，属于正常现象，不影响应用启动。

## 0. 出厂备份

备份已经在 `backups/`：

```text
backups/coloplay-factory-20261113-131459.bin
SHA-256: 9c73cc4e389bd798a883dbcd4102ab8144b2e3744c81d004841acd0bf89ad46e
```

恢复出厂固件：

```bash
esptool --port /dev/cu.usbmodemXXXX --baud 460800 \
  write-flash 0 backups/coloplay-factory-20261113-131459.bin
```

## 1. 编译

```bash
./scripts/build_firmware.sh
```

应用产物：`firmware/.pio/build/coloplay/firmware.bin`。

工程使用：

- `board_build.arduino.memory_type = qio_opi`（N16R8 的八线 PSRAM）
- `board_build.partitions = partitions_coloplay.csv`（与原厂分区偏移一致）

## 2. 首次 USB 刷写（保留原厂 bootloader）

进入 ROM 下载模式：

1. 拔掉 USB；如果拔电后屏幕还亮，需要断开电池，确保 ESP32 完全断电。
2. 按住电源板 **REC_KEY**，同时按下中间 **旋钮**。
3. 保持按住，插回 USB。
4. 约 1 秒后松开。

然后运行：

```bash
./scripts/flash_bootstrap.py
```

脚本会等待 `303A:1001` 的 ROM USB-JTAG 接口，然后：

- 把原厂分区表复制到 `0x8000`（Arduino 应用从 `0x8000` 读取分区表）
- 把应用写到 `0x20000`
- 不覆盖 `0x0` 的原厂 bootloader 和 `0x10000` 的原厂分区表

## 3. 验证

串口 115200 应出现：

```text
ColoPLAY bring-up firmware 0.1.x
[chip] ESP32-S3 ...
[wifi] connected: ip=... rssi=... mac=...
[mdns] http://coloplay.local/
[ota] ArduinoOTA ready
[http] server listening on port 80
```

然后：

```bash
curl http://coloplay.local/api/status
```

本次实测 IP 为 `10.0.0.225`，`/api/status` 返回：

```json
{"firmware":"0.1.3-nvs","wifi":{"connected":true,"stored_ssid":"OnIoT","source":"nvs","ip":"10.0.0.225"}}
```

## 4. WiFi 凭据（NVS）

凭据保存在 NVS 的 `wifi` 命名空间（键 `ssid` / `pass`），不再每次从编译常量读取：

- 首次启动时 NVS 为空，会从 `firmware/include/secrets.h` 播种一次
- 之后以 NVS 为准，重启 / OTA 都不会丢失
- 网页底部有修改表单；保存后设备自动重启
- `GET /api/wifi` 返回当前存储的 SSID（不返回密码）

命令行修改：

```bash
curl -X POST --data-urlencode 'ssid=OnIoT' \
  --data-urlencode 'password=<password>' \
  http://coloplay.local/api/wifi
```

如果新凭据写错导致 WiFi 连不上，用 USB 串口恢复（115200）：

```text
wifi-reset
```

会清空 NVS 的 `wifi` 命名空间并重启，下次启动重新从 `secrets.h` 播种。
也可以直接设置新凭据：

```text
wifi <ssid> <password>
```

其他串口命令：

```text
status
```

注意：NVS 分区就是原厂的 `nvs`（`0x11000`）。恢复原厂 16 MB 备份会同时恢复
原厂 NVS，自定义 WiFi 配置会被覆盖。

## 5. OTA 升级（已验证）

```bash
./scripts/ota_upload.sh 10.0.0.225
```

脚本从 `firmware/include/secrets.h` 读取 `OTA_PASSWORD`，调用 Arduino
`espota.py` 上传当前 `firmware.bin`。

升级过程：

1. ArduinoOTA 选择当前未运行的槽：
   - 当前在 app0（`0x20000`）→ 写入 app1（`0x4a0000`）
   - 当前在 app1（`0x4a0000`）→ 写入 app0（`0x20000`）
2. 更新 `otadata`
3. 重启后原厂 bootloader 从新槽启动

串口会显示：

```text
[ota] update started (firmware)
[ota] 100%
[ota] update finished
```

已实测两个方向：

```text
Loaded app from partition at offset 0x4a0000
Loaded app from partition at offset 0x20000
```

当前运行版本为 `0.1.3-nvs`，WiFi 凭据从 NVS 读取。

## 6. 恢复出厂

```bash
esptool --port /dev/cu.usbmodemXXXX --baud 460800 \
  write-flash 0 backups/coloplay-factory-20261113-131459.bin
```

## 7. 已知限制

- 屏幕、音频、按键灯、旋钮尚未驱动；这是刻意的最小固件。
- `psram_bytes` 显示 0：当前 PSRAM 初始化未启用/未验证，不影响 WiFi/OTA。
- 原厂 bootloader 的 SHA-256 警告不可消除，除非替换 bootloader；
  目前它选择继续启动，OTA 已实测可用。
- `secrets.h` 里的 WiFi 凭据仍会编译进固件，但只用于首次播种；运行时以 NVS 为准。
  `firmware/include/secrets.h` 已在 `.gitignore` 中，不会提交。
- 如果希望固件完全不包含 WiFi 密码，可以清空 `WIFI_SSID` / `WIFI_PASSWORD`，
  首次启动后用串口 `wifi <ssid> <password>` 配网。
- 如果后续固件需要显示，使用 GC9A01 + 已确认的 SPI 引脚：
  CLK 14、MOSI 13、CS 12、DC/RS 11、RST 45、BLK 18、TE 17。
