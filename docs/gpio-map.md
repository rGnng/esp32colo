# ColoPLAY V1 GPIO 分配

依据：

- ColoPLAY 主板原理图（`docs/coloplay-main.pdf`，官方 `Colo Play_Main_Project`）
- 屏幕转接板原理图（`docs/coloplay-screen.pdf`）
- 电源板原理图（`docs/coloplay-power.pdf`）
- ESP32-S3-WROOM-1 数据手册（`docs/esp32-s3-wroom-1-datasheet.pdf`）

模块为 **ESP32-S3-WROOM-1-N16R8**，下表把模块引脚、GPIO 编号和网络名对应起来。
这是从原理图读出的设计值，不是对当前样机的实测值。

## 总表

| GPIO | 模块脚 | 网络名 | 外设 / 用途 | 备注 |
|---:|---:|---|---|---|
| 0 | 27 | `REC_KEY` / `IO0` | 电源板 S1 按键（REC） | 启动 strapping 脚，低电平进入下载模式 |
| 1 | 39 | `KEY2` | 主板上第 2 个按键 | 按键另一端到 GND，10K 上拉 |
| 2 | 38 | `AUD_CTRL` | CS8120 功放使能/静音 | 高/低有效需实测 |
| 3 | 15 | `CHRG` | TP4055 充电状态 | 启动 strapping 脚（JTAG 源） |
| 4 | 4 | `KEY3` | 主板上第 3 个按键 | 10K 上拉 |
| 5 | 5 | `KEY4` | 主板上第 4 个按键 | 10K 上拉 |
| 6 | 6 | `KEY1` | 主板上第 1 个按键 | 10K 上拉 |
| 7 | 7 | `VOL_ADC` | ADC1_CH6，2M/1M 分压 | 具体用途待确认（音量/版本检测） |
| 8 | 12 | `MICBIAS` | 麦克风偏置使能 | 经 Q6/Q7 控制 MICBIAS |
| 9 | 17 | `EAS` | 旋钮编码器 A 相 | 屏幕转接板 S1 |
| 10 | 18 | `EBS` | 旋钮编码器 B 相 | 屏幕转接板 S1 |
| 11 | 19 | `LCD_RS` | LCD 数据/命令（DC） | SPI |
| 12 | 20 | `LCD_CS` | LCD 片选 | SPI |
| 13 | 21 | `LCD_MOSI` | LCD 数据 | SPI，无 MISO |
| 14 | 22 | `LCD_CLK` | LCD 时钟 | SPI |
| 15 | 8 | `OPEN_KEY` | 电源板 S2 按键（OPEN） | 10K 上拉 |
| 16 | 9 | `LIGHTADC` | ADC2_CH5，环境光传感器 | D14 光电二极管 + R26 510K |
| 17 | 10 | `LCD_TE` | LCD tearing effect | 可不用 |
| 18 | 11 | `LCD_BLK` | LCD 背光使能 | 经 Q1 9013 驱动 LEDK |
| 19 | 13 | `USB_N` | USB D- | 原生 USB |
| 20 | 14 | `USB_P` | USB D+ | 原生 USB |
| 21 | 23 | `AUD_SDOUT` | I2S 数据：ESP32 → ES8311 | ES8311 pin 9 DSDIN |
| 22 | — | — | 未使用 | — |
| 23 | — | — | 未使用 | — |
| 24 | — | — | 未使用 | — |
| 25 | — | — | 未使用 | — |
| 26 | — | — | 未使用 | — |
| 27 | — | — | 未使用 | — |
| 28 | — | — | 未使用 | — |
| 29 | — | — | 未使用 | — |
| 30 | — | — | 未使用 | — |
| 31 | — | — | 未使用 | — |
| 32 | — | — | 未使用 | — |
| 33 | — | — | 未使用 | — |
| 34 | — | — | 未使用 | — |
| 35 | 28 | — | Octal PSRAM | N16R8 内部占用，不可外设 |
| 36 | 29 | — | Octal PSRAM | 同上 |
| 37 | 30 | — | Octal PSRAM | 同上 |
| 38 | 31 | `AUD_SDIN` | I2S 数据：ES8311 → ESP32 | ES8311 pin 7 ASDOUT |
| 39 | 32 | `AUD_BCLK` | I2S 位时钟 | ES8311 pin 6 SCLK |
| 40 | 33 | `AUD_MCLK` | I2S 主时钟 | ES8311 pin 2 MCLK |
| 41 | 34 | `AUD_SCL` | ES8311 I2C 时钟 | ES8311 pin 1 CCLK |
| 42 | 35 | `AUD_SDA` | ES8311 I2C 数据 | ES8311 pin 19 CDATA |
| 43 | 37 | `UART0_TX` | 调试串口 TX | 下载排针 P1 |
| 44 | 36 | `UART0_RX` | 调试串口 RX | 下载排针 P1 |
| 45 | 26 | `LCD_REST` | LCD 复位 | 启动 strapping 脚（VDD_SPI） |
| 46 | 16 | `EKEY` | 旋钮按下 | 启动 strapping 脚，外部上拉 |
| 47 | 24 | `AUD_WS` | I2S 左右声道时钟（LRCK） | ES8311 pin 8 LRCK |
| 48 | 25 | `RGB_LED` | 按键/氛围灯 WS2812 数据 | 4 颗 2121 + 9 颗 1204 灯珠 |

## 按外设分组

### LCD（SPI，240×240 圆屏）

| 信号 | GPIO |
|---|---:|
| `LCD_CLK` | 14 |
| `LCD_MOSI` | 13 |
| `LCD_CS` | 12 |
| `LCD_RS` / DC | 11 |
| `LCD_REST` | 45 |
| `LCD_BLK`（背光） | 18 |
| `LCD_TE` | 17 |

原理图只画到 FPC 连接器，没有显示控制器型号；ColoX 文档说明 ColoPLAY V1 是
**240×240 圆屏**。出厂固件中已确认 **GC9A01**（见
[factory-firmware.md](factory-firmware.md)），背光由 LEDC PWM 控制。
SPI 没有 MISO，属于只写屏幕。

### 音频（ES8311 + CS8120）

| 信号 | GPIO |
|---|---:|
| `AUD_MCLK` | 40 |
| `AUD_BCLK` | 39 |
| `AUD_WS` / LRCK | 47 |
| `AUD_SDIN`（codec → ESP32） | 38 |
| `AUD_SDOUT`（ESP32 → codec） | 21 |
| `AUD_SCL` | 41 |
| `AUD_SDA` | 42 |
| `AUD_CTRL`（功放） | 2 |
| `MICBIAS` | 8 |

出厂固件确认使用 **ES8311** codec 与 I2S0/I2S1 驱动，见
[factory-firmware.md](factory-firmware.md)。

### 按键与旋钮

| 功能 | GPIO | 位置 |
|---|---:|---|
| `KEY1` | 6 | 主板 S1 |
| `KEY2` | 1 | 主板 S2 |
| `KEY3` | 4 | 主板 S3 |
| `KEY4` | 5 | 主板 S4 |
| `OPEN_KEY` | 15 | 电源板 S2 |
| `REC_KEY` | 0 | 电源板 S1 |
| `EKEY`（旋钮按下） | 46 | 屏幕转接板 S1 |
| `EAS`（编码器 A） | 9 | 屏幕转接板 S1 |
| `EBS`（编码器 B） | 10 | 屏幕转接板 S1 |

### ADC / 电源

| 功能 | GPIO | 说明 |
|---|---:|---|
| `VOL_ADC` | 7 | ADC1_CH6，2M/1M 分压 |
| `LIGHTADC` | 16 | ADC2_CH5，光敏二极管 |
| `CHRG` | 3 | TP4055 充电状态 |

### USB / 调试

| 信号 | GPIO |
|---|---:|
| USB D- | 19 |
| USB D+ | 20 |
| UART0 TX | 43 |
| UART0 RX | 44 |

## 启动 strapping 脚注意事项

- **GPIO0**（`REC_KEY`）：低电平 + 复位进入 ROM 下载模式。
- **GPIO46**（`EKEY`）：下载模式下也需为低或悬空；板上有外部上拉。
- **GPIO45**（`LCD_REST`）：VDD_SPI 电压 strapping，启动时内部弱下拉。
- **GPIO3**（`CHRG`）：JTAG 信号源 strapping。

因此进入下载模式的标准动作是：**按住 REC_KEY + 按下旋钮，再上电/插 USB**。

## 待确认项

1. LCD 控制器型号（GC9A01 或其他）。
2. `AUD_CTRL` 的有效电平（功放使能还是静音）。
3. `VOL_ADC` 的实际用途（电位器、版本检测还是电池检测）。
4. 4 颗按键灯与 9 颗氛围灯是否由同一路 `RGB_LED` 并联驱动。
