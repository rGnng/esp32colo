# 原厂固件备份与分析

## 备份

| 项目 | 值 |
|---|---|
| 文件 | `backups/coloplay-factory-20261113-131459.bin` |
| 大小 | 16 MiB（16,777,216 字节） |
| SHA-256 | `9c73cc4e389bd798a883dbcd4102ab8144b2e3744c81d004841acd0bf89ad46e` |

读取时芯片信息：

- ESP32-S3（QFN56，revision v0.2）
- MAC `58:e6:c5:72:a4:ac`
- 16 MB flash，quad（4 data lines），3.3 V
- 8 MB embedded PSRAM
- **Secure Boot: Disabled**
- **Flash Encryption: Disabled**
- `SPI_BOOT_CRYPT_CNT: 0x0`

因此这份备份是可读、可恢复的完整出厂镜像。

## 分区表

原厂分区表不在常见的 `0x8000`，而在 **`0x10000`**（自定义
`CONFIG_PARTITION_TABLE_OFFSET`）。内容如下：

| 分区 | 类型 | 偏移 | 大小 |
|---|---|---:|---:|
| `nvs` | data/nvs | `0x11000` | `0x4000` (16 KiB) |
| `otadata` | data/ota | `0x15000` | `0x2000` (8 KiB) |
| `phy_init` | data/phy | `0x17000` | `0x9000` (36 KiB) |
| `ota_app0` | app/ota_0 | `0x20000` | `0x480000` (4.5 MiB) |
| `ota_app1` | app/ota_1 | `0x4a0000` | `0x480000` (4.5 MiB) |
| `user_bin` | data/6 | `0x920000` | `0x100000` (1 MiB) |
| `lfs` | 194/1 | `0xa20000` | `0x40000` (256 KiB) |
| `littlefs` | data/130 | `0xa60000` | `0x5a0000` (5.6 MiB) |

## 应用信息

`ota_app0` 的 ESP-IDF 应用描述：

| 字段 | 值 |
|---|---|
| project | `nodemcu` |
| version | `1` |
| build date | `Aug  7 2026` |
| build time | `15:19:48` |
| IDF | `HEAD-HASH-NOTFOUND`（厂商定制 IDF） |

## 固件组件（字符串证据）

在 `ota_app0` 中可以直接找到：

- **`GC9A01`** — 240×240 圆屏 LCD 控制器
- `lvgl_helpers`、`lvgl_tft/disp_spi.c`、`esp_lcd_backlight.c`
- `ledc_timer_config(&LCD_backlight_timer)`、`Setting LCD backlight: %d%%`
- **`ES8311`**、`es8311_codec_init`、`es8311_set_volume`、`es8311_set_micgain`
- `i2s_es8311`、`i2s_driver_init`、`I2S0` / `I2S1`
- `ColoX` Lua VM、`__ColoX-BIOS.lua`、`coloX-4` / `coloX-16` 规格
- `ColoPLAY1.ver`、`COLOPLAY.bin`、`LifeSmartXzYq:COLOPLAY`

这确认了显示和音频的硬件选型，也解释了原厂固件的结构：ColoX Lua VM +
LVGL + GC9A01 + ES8311。

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

```bash
esptool --port /dev/cu.usbmodemXXXX --baud 460800 \
  write-flash 0 backups/coloplay-factory-20261113-131459.bin
```

恢复后原厂 bootloader、分区表、应用、NVS 和 LittleFS 都会回到备份时的状态。
