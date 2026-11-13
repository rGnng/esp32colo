# 原厂固件备份与分析

本文只记录保存的原厂镜像、随备份附带的设备日志及其解析结果，不使用硬件图纸。
分区内容、空白区域与 OTA 选择描述的是原始备份，不是当前自定义固件的运行状态。
当前操作入口：[固件指南](firmware.md)、[构建与恢复](flash.md)；
原厂引脚与协议证据：[GPIO](firmware-gpio-evidence.md)、[LED](firmware-led-evidence.md)。

## 备份

2026-11-13 从用户上传的 `cololight_firmware_backups.7z` 导入。
压缩包、原始镜像、设备日志、校验文件及派生分析都保存在本地 `backups/`，
该目录由 `.gitignore` 整体忽略。原始镜像未修改。

| 项目 | 值 |
|---|---|
| 文件 | `backups/coloplay-factory-20261113-131459.bin` |
| 大小 | 16 MiB（16,777,216 字节） |
| SHA-256 | `9c73cc4e389bd798a883dbcd4102ab8144b2e3744c81d004841acd0bf89ad46e` |

导入后重新计算的 SHA-256 与上传的 `.bin.sha256` 一致。
压缩包自身 SHA-256 为
`c6ed2f2bd71df73ec414c6565da31164b0143f67cf87ae0b592c161d1935d779`。

以下是同一压缩包内 `*-flash-id.txt` / `*-security.txt` 的备份时日志，
本次没有重新连接设备读取 eFuse：

- ESP32-S3（QFN56，revision v0.2）
- 设备 MAC 保存在本地 `*-mac.txt` 日志
- 16 MB flash，quad（4 data lines），3.3 V
- 8 MB embedded PSRAM
- **Secure Boot: Disabled**
- **Flash Encryption: Disabled**
- `SPI_BOOT_CRYPT_CNT: 0x0`

这支持它是一份可解析的完整 16 MiB Flash 备份；**不代表恢复启动已经实测成功**。
Flash 镜像也不是 eFuse、安全密钥或外部硬件状态的备份。

## 完整性与 bootloader 头部差异

- 分区表位于 `0x10000`；ESP-IDF 解析器验证其 MD5 通过，八个分区均不重叠且落在 16 MiB 内。
- `ota_app0` 的镜像 XOR 校验及追加 SHA-256 均有效。
- bootloader 位于 `0x0`，镜像总长 `0x5740` 字节；XOR 校验有效，
  **追加 SHA-256 与当前头部不匹配**。
- 该差异能被一个字节完整解释：仅在内存中把 bootloader 头部偏移 `0x03`
  从 `0x40` 改为 `0x4f`，计算结果就与保存的 SHA-256 完全一致。
  两者都是 16 MB / DIO，频率分别为 **40 MHz** 和 **80 MHz**。
  这与烧录时覆盖 Flash 频率头部相符；实际发生原因没有独立日志证明。
- 原始备份仍保留 `0x40`，没有为让校验通过而“修复”或重写它。

bootloader 保存的 SHA-256：
`f61e15ff4869172de4906ecf0f2a56f17326551bed45fb5c3665539f8cb18642`。
对当前头部实际计算的值：
`60d4ed9820c05028bb3dd3302f19356be9ec16f0865e33671e31eb534c6d208f`。

`ota_app0` 追加 SHA-256：
`30a16e2f1a59d74676009fa48a88af10631be8db3d80d4006220d32ead3891e3`。
这些是镜像内部摘要，与上表的整个 16 MiB 文件 SHA-256 不同。

## 分区表

原厂分区表不在常见的 `0x8000`，而在 **`0x10000`**（自定义
`CONFIG_PARTITION_TABLE_OFFSET`）。内容如下：

| 分区 | 类型 | 偏移 | 大小 |
|---|---|---:|---:|
| `nvs` | data/nvs | `0x11000` | `0x4000` (16 KiB) |
| `otadata` | data/ota | `0x15000` | `0x2000` (8 KiB) |
| `phy_init` | data/phy | `0x17000` | `0x9000` (36 KiB，全 `0xff`) |
| `ota_app0` | app/ota_0 | `0x20000` | `0x480000` (4.5 MiB) |
| `ota_app1` | app/ota_1 | `0x4a0000` | `0x480000` (4.5 MiB，全 `0xff`) |
| `user_bin` | data/6 | `0x920000` | `0x100000` (1 MiB) |
| `lfs` | 自定义 `0xc2/0x01` | `0xa20000` | `0x40000` (256 KiB，全 `0xff`) |
| `littlefs` | data/`0x82` | `0xa60000` | `0x5a0000` (5.625 MiB) |

`littlefs` 分区起始 `+0x08` 处存在 `littlefs` superblock 标识。其 `0x82` 子类型沿用
旧式命名约定；新版 ESP-IDF 解析器可能因该子类型在当前 SDK 中名为 `spiffs` 而警告，
这不等于分区表 MD5 损坏。`lfs` 只是一个自定义类型的空分区，不能仅按标签认定用途。

### OTA 选择

`otadata` 的首条记录（`0x15000`）为 sequence=1、state=2（VALID）、
CRC=`0x4743989a`，CRC 校验通过；第二条记录（`0x16000`）全 `0xff`。
按标准 ESP-IDF OTA 选择规则，当前指向 **`ota_app0`**。`ota_app1` 没有第二份可启动应用。

## 应用信息

`ota_app0` 的 ESP-IDF 应用描述：

| 字段 | 值 |
|---|---|
| project | `nodemcu` |
| version | `1` |
| build date | `Aug  7 2026` |
| build time | `15:19:48` |
| IDF | `HEAD-HASH-NOTFOUND`（未提供可识别的 SDK 版本） |

版本字符串缺失本身不能证明厂商修改了 ESP-IDF，也不能据此选择具体 SDK 版本。

## 固件组件（字符串证据）

在 `ota_app0` 中可以直接找到：

- **`GC9A01`** — 240×240 圆屏 LCD 控制器
- `lvgl_helpers`、`lvgl_tft/disp_spi.c`、`esp_lcd_backlight.c`
- `ledc_timer_config(&LCD_backlight_timer)`、`Setting LCD backlight: %d%%`
- **`ES8311`**、`es8311_codec_init`、`es8311_set_volume`、`es8311_set_micgain`
- `i2s_es8311`、`i2s_driver_init`、`I2S0` / `I2S1`
- `ColoX` Lua VM、`__ColoX-BIOS.lua`、`coloX-4` / `coloX-16` 规格
- `ColoPLAY1.ver`、`COLOPLAY.bin`、`LifeSmartXzYq:COLOPLAY`

进一步的反汇编追踪到显示初始化实际调用 GC9A01 驱动，恢复了 LCD、I2S、I2C、
编码器及按键表配置，见 [firmware-gpio-evidence.md](firmware-gpio-evidence.md)。
证据支持原厂软件使用 ColoX / LVGL / GC9A01 / ES8311 路径；具体面板变体、
未恢复的外设运行参数仍不能只凭字符串认定。

### LED 发送路径（原厂代码与数据）

原厂 Lua 明确调用 `qt.init(48)` 并创建 `qt.new(14)`。已追踪 `qt:write()` 的
绑定表、编码函数和 SPI 配置，恢复出 **GPIO48 / SPI3 / 3.2 MHz / GRB / 14 槽帧**。
每个 LED 数据位编码为 `1000` 或 `1100`，480 字节传输含前后低电平复位填充。
这项结论来自固件二进制，由原厂代码和初始化数据恢复。

地址、查表值和重现脚本见 [LED 固件拆解](firmware-led-evidence.md)。
复制该格式的 `0.2.4-factory-led` 已通过 HTTP OTA 安装；用户确认环灯与四键灯都工作。
“14 槽”是软件容量，不单独据此断定物理上安装了 14 颗 LED。

## Bootloader 与自定义固件

原厂 bootloader 位于 `0x0`，分区表在 `0x10000`，应用槽在 `0x20000`
（`ota_app0`）和 `0x4a0000`（`ota_app1`）。

这台机器的 flash 芯片 Manufacturer ID 为 `0x46`，Arduino 默认 bootloader
在这颗芯片上无法正常启动（反复 `TG0WDT_SYS_RST`）。因此自定义固件保留原厂
bootloader，只替换应用：

- 在 `0x8000` 写入原厂分区表的副本，供 Arduino 应用读取
- 在 `0x20000` 写应用
- `0x0`、`0x10000`、NVS、PHY、OTA 数据都保持不变

原厂 bootloader 启动时会打印：

```text
SHA-256 comparison failed:
Calculated: ...
Expected: ...
Attempting to boot anyway...
```

这是 bootloader 对自身镜像头的校验警告；它随后继续启动。实测 OTA 写入
`0x4a0000` 后，bootloader 正常从该槽启动。

## 恢复

仅在同一设备的下载模式下、已确认当前安全配置允许恢复时使用；本次未执行。
以下为 esptool 5.x 命令，`SERIAL_PORT` 需换成实际端口（Linux 通常为
`/dev/ttyACM0`，macOS 通常为 `/dev/cu.usbmodem...`）：

```bash
SERIAL_PORT=/dev/ttyACM0
esptool --chip esp32s3 --port "$SERIAL_PORT" --baud 460800 --after no-reset \
  write-flash --flash-mode keep --flash-freq keep --flash-size keep \
  0x0 backups/coloplay-factory-20261113-131459.bin
esptool --chip esp32s3 --port "$SERIAL_PORT" --before no-reset --after no-reset \
  verify-flash 0x0 backups/coloplay-factory-20261113-131459.bin
```

先保留在下载模式完成比较，再正常断电重启，以免原厂程序运行后更新 NVS 或 OTA 元数据，
影响逐字节比较。完整写回会覆盖当前 Flash 中的 bootloader、分区、应用、NVS 和文件系统；
不会还原 eFuse，也会覆盖自定义固件在 NVS 中保存的 WiFi、时钟、USB、场景和灯光配置。
由于上述 bootloader 头部摘要差异及尚未进行的恢复试验，不把“可解析备份”等同于“已验证可恢复”。

参考：[ESP-IDF 分区表](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-guides/partition-tables.html)、
[OTA 元数据](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/api-reference/system/ota.html)、
[esptool 镜像及 Flash 命令](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/basic-commands.html)。
