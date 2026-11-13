# ColoPLAY 刷机流程

`firmware/` 当前构建 **`0.2.4-factory-led` 硬件演示固件**：屏幕、RGB 灯、按键、旋钮、
ES8311 音频、ADC / 充电状态和 PSRAM 检查，并增加带密码的 HTTP OTA。
本地构建和主机 OTA 测试通过，已通过 HTTP OTA 部署并核验运行镜像 MD5。
该版修复 `0.2.0-demo` 的 TFT_eSPI / S3 SPI 寄存器索引崩溃；真机硬件初始化完成，
有屏幕 TE 脉冲、帧缓冲图像、PSRAM 测试和 ES8311 麦克风数据。LED 驱动已从原厂
固件恢复为 SPI3 3.2 MHz、GRB、14 槽帧。用户已确认画面、声音、环灯和四键灯正常；
四键及旋钮事件也已观测到。见 [LED 二进制拆解](firmware-led-evidence.md)。
完整命令、控制和恢复方法见 [硬件演示指南](hardware-demo.md)。

```bash
git pull --ff-only
./scripts/build_firmware.sh && ./scripts/ota_upload.sh 10.0.0.225
```

首次从设备同一 LAN 运行，保留现有 `secrets.h`。安装后可通过
`./scripts/ota_upload.sh http://127.0.0.1:10081` 更新，或浏览器访问 `/update`
（用户 `admin`，密码为 `OTA_PASSWORD`）。保留原厂 bootloader，只更新应用。

下方保留已经在真机确认的 `0.1.3-nvs` 最小 bring-up 记录；其当时功能为：

- 连接 `OnIoT` WiFi（凭据保存在 NVS，首次启动从 `secrets.h` 播种）
- `http://coloplay.local/` 状态页，`/api/status` JSON
- ArduinoOTA 无线升级
- 读取 GPIO0 / GPIO15 / GPIO46 按键状态
- **不初始化屏幕、音频等外设**，用于先建立 WiFi + OTA 通道

## 环境与离线分析记录

2026-11-13 上传的完整原厂镜像已导入本地 `backups/`，其 SHA-256 与校验文件一致。
LCD、按键表、编码器和 I2S 配置已从原厂代码恢复，见
[GPIO 证据](firmware-gpio-evidence.md)。GPIO17 / TE 的实际固件使用仍未在该次分析中确认。
该次分析本身没有连接设备或刷写；下文保留仓库后续记录的 app-only / OTA 实测结果。

本文件示例使用 esptool 5.x，备份等待脚本要求 esptool >= 5.4。
[LCD RAM probe](../firmware/lcd_probe/README.md) 使用独立 ESP-IDF 环境和 esptool 4.13.0，
应按其 `load_ram` 指令运行；不能将 RAM 镜像作为持久化应用烧录。

Linux 串口通常为 `/dev/ttyACM*`，macOS 通常为 `/dev/cu.usbmodem*`。
文中的端口和 IP 都需要换成实际设备地址。

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

仓库记录本机在此警告后继续启动应用并完成 OTA。离线分析也发现了相符的头部差异：
原厂 bootloader 的 Flash 频率字段是 40 MHz，而它保存的摘要对应 80 MHz 头部；
仅在内存中替换该字节即可重现保存的 SHA-256。原始备份没有改动。
这是对这份镜像的具体结论，不能将所有 SHA-256 失败都视为可忽略。
详见 [完整性分析](factory-firmware.md#完整性与-bootloader-头部差异)。

## 0. 出厂备份

备份已经在 `backups/`：

```text
backups/coloplay-factory-20261113-131459.bin
SHA-256: 9c73cc4e389bd798a883dbcd4102ab8144b2e3744c81d004841acd0bf89ad46e
```

恢复出厂固件：

按 [原厂备份文档的完整写回与校验步骤](factory-firmware.md#恢复)，
从 `0x0` 写回原始全片镜像并保留其头部参数。完整恢复会覆盖自定义 NVS WiFi 配置，
且不会还原 eFuse。完整原厂恢复启动尚未实测。

## 1. 编译

先安装 PlatformIO 和脚本所需版本的 esptool，并把环境加入 PATH。
本地尚无 `secrets.h` 时，复制示例并设置 WiFi / OTA 凭据；已有文件应保留：

```bash
cp -n firmware/include/secrets.example.h firmware/include/secrets.h
# 编辑 firmware/include/secrets.h；该文件已被 git ignore。
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

这与早期替换 bootloader 的合并镜像方案不同。不要使用旧的 `firmware-merged.bin`，
或未经核对的默认 PlatformIO 全量 upload 去代替此脚本。应用地址是 `0x20000`；
`0x10000` 是原厂分区表，不是此方案的应用地址。脚本的 Flash 写入不改 NVS，
但应用运行后的首次播种或用户配网会更新 NVS。

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

## 5. ArduinoOTA 升级（0.1.3 已验证）

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

本次硬件演示开发前最后确认的运行版本为 `0.1.3-nvs`，WiFi 凭据从 NVS 读取。
新上传脚本还会校验应用文件并等待设备报告匹配的 `sketch_md5`；HTTP OTA 和
新硬件驱动的验证步骤见 [hardware-demo.md](hardware-demo.md)。

## 6. 恢复出厂

按 [原厂备份文档的完整写回与校验步骤](factory-firmware.md#恢复)，
从 `0x0` 写回原始全片镜像并保留其头部参数。完整恢复会覆盖自定义 NVS WiFi 配置，
且不会还原 eFuse。完整原厂恢复启动尚未实测。

## 7. 已知限制

- `0.1.3-nvs` 不驱动屏幕、音频、灯和旋钮；当前演示版已确认画面、声音、环灯与四键灯正常。
- `0.1.3-nvs` 的 `psram_bytes` 为 0；演示版启用 `BOARD_HAS_PSRAM` 并增加内存模式测试。
- 原厂 bootloader 的 SHA-256 头部差异已被离线定位；本机记录为继续启动且 OTA 可用。
  当前流程保留这份已能工作的 bootloader，没有尝试修改其头部或替换它。
- `secrets.h` 里的 WiFi 凭据仍会编译进固件，但只用于首次播种；运行时以 NVS 为准。
  `firmware/include/secrets.h` 已在 `.gitignore` 中，不会提交。
- 如果希望固件完全不包含 WiFi 密码，可以清空 `WIFI_SSID` / `WIFI_PASSWORD`，
  首次启动后用串口 `wifi <ssid> <password>` 配网。
- 显示使用 GC9A01 + 已确认的 SPI 引脚：
  CLK 14、MOSI 13、CS 12、DC/RS 11、RST 45、BLK 18。演示版已在 GPIO17 观测到 TE 脉冲；
  原厂 TE 使用情况未单独恢复。原厂显示 SPI 配置为 80 MHz、mode 0、MISO=-1。
  缺少独立 MISO 不等于已证明不能在 SDA/MOSI 上半双工读回。
