# ColoPLAY V1 GPIO 分配

依据：

- ColoPLAY 主板原理图（`docs/coloplay-main.pdf`，官方 `Colo Play_Main_Project`）
- 屏幕转接板原理图（`docs/coloplay-screen.pdf`）
- 电源板原理图（`docs/coloplay-power.pdf`）
- ESP32-S3-WROOM-1 数据手册（`docs/esp32-s3-wroom-1-datasheet.pdf`）

模块为 **ESP32-S3-WROOM-1-N16R8**，下表把模块引脚、GPIO 编号和网络名对应起来。
这是原理图的物理网络映射，不是当前样机的逐脚测量结果。

2026-11-13 已对上传的完整出厂镜像做静态反汇编核对：LCD、编码器、六键扫描表、
I2S 引脚及 I2C 配置均与下述相关网络相符。固件的键扫描顺序并不等于 `KEY1` 到
`KEY4` 的网名顺序，见下面的按键表。证据地址、镜像 SHA-256 和静态分析方法见
[firmware-gpio-evidence.md](firmware-gpio-evidence.md)。未在二进制中恢复出配置的信号
仍只按原理图记录，不能将整张表都视为固件实测值。

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
| 7 | 7 | `VOL_ADC` | ADC1_CH6，VDD 供电电压采样 | R37 2M / R38 1M，约 VDD/3；并非音量电位器 |
| 8 | 12 | `MICBIAS` | 麦克风偏置使能 | 经 Q6/Q7 控制 MICBIAS |
| 9 | 17 | `EAS` | 旋钮编码器 A 相 | 屏幕转接板 EC1；原厂 PCNT 配置也为 GPIO9/10 |
| 10 | 18 | `EBS` | 旋钮编码器 B 相 | 屏幕转接板 EC1 |
| 11 | 19 | `LCD_RS` | LCD 数据/命令（DC） | SPI |
| 12 | 20 | `LCD_CS` | LCD 片选 | SPI |
| 13 | 21 | `LCD_MOSI` | LCD 数据 | 原厂 MOSI=13、MISO=-1；单线读回能力仍取决于面板 |
| 14 | 22 | `LCD_CLK` | LCD 时钟 | SPI |
| 15 | 8 | `OPEN_KEY` | 电源板 S2 按键（OPEN） | 图中未见外部 10K 上拉；原厂固件请求内部上拉 |
| 16 | 9 | `LIGHTADC` | ADC2_CH5，环境光传感器 | D14 光电二极管 + R26 510K |
| 17 | 10 | `LCD_TE` | LCD tearing effect | 可不用 |
| 18 | 11 | `LCD_BLK` | LCD 背光使能 | 经 Q1 9013 驱动 LEDK |
| 19 | 13 | `USB_N` | USB D- | 原生 USB |
| 20 | 14 | `USB_P` | USB D+ | 原生 USB |
| 21 | 23 | `AUD_SDOUT` | I2S 数据：ESP32 → ES8311 | ES8311 pin 9 DSDIN |
| 22–25 | — | — | ESP32-S3 无这些 GPIO 编号 | 不是空闲引脚 |
| 26–32 | — | 内部 SPI0/1 | 模块内部 Flash / PSRAM 总线 | 未作为模块外设引脚引出，不能当作空闲 GPIO |
| 33–34 | — | Octal PSRAM 内部连接 | N16R8 内部占用 | 未引出，不能用于外设 |
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
| 48 | 25 | `RGB_LED` | 按键/氛围灯数据 | 图中经 R35/R36 分别驱动 4 颗和 9 颗灯，协议待实测 |

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
**240×240 圆屏**。出厂镜像的显示初始化路径明确调用 **GC9A01** 驱动，并配置
MOSI=13、SCLK=14、CS=12、DC=11、RESET=45；GPIO18 背光配置为 PWM、输出不反相。
原厂 SPI 设置为 **80 MHz、mode 0、MISO=-1**。这不等于新固件必须一开始就跑 80 MHz，
也不是单凭字符串就测得了具体面板型号。证据见
[firmware-gpio-evidence.md](firmware-gpio-evidence.md)。

没有独立 MISO 线不能证明面板只能写：部分 LCD 可以在 SDA/MOSI 上半双工读回。
该面板实际是否支持这种接口仍未实测；GPIO17/TE 的固件使用也未在本次显示配置中确认。

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

出厂固件包含 **ES8311** codec 路径；I2S 配置结构的五个引脚值为
`40, 39, 47, 21, 38`，I2C 配置为 SDA=42、SCL=41，与原理图一致。
不能仅凭 `I2S0` / `I2S1` 字符串就认定两个端口都在实际使用，见
[firmware-gpio-evidence.md](firmware-gpio-evidence.md)。

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
| `EAS`（编码器 A） | 9 | 屏幕转接板 EC1 |
| `EBS`（编码器 B） | 10 | 屏幕转接板 EC1 |

### 原厂固件的六键扫描顺序

原厂输入表位于运行时地址 `0x3fca0ad4`，初始化代码扫描六个表项：

| 表项索引 | GPIO | 原理图网络 |
|---:|---:|---|
| 0 | 6 | `KEY1` |
| 1 | 1 | `KEY2` |
| 2 | 5 | `KEY4` |
| 3 | 4 | `KEY3` |
| 4 | 46 | `EKEY`（旋钮按下） |
| 5 | 15 | `OPEN_KEY` |

因此 **表项 2 是 KEY4 / GPIO5，表项 3 是 KEY3 / GPIO4**。
这不会改变原理图中 `KEY3`→4、`KEY4`→5 的物理网名。
表项索引是否直接对应 ColoX Lua 的 `btn()` 编号，还需确认上层事件转换。
GPIO0 / `REC_KEY` 不在这个六键扫描表中，仍保留其原理图和 ROM 下载用途。

### ADC / 电源

| 功能 | GPIO | 说明 |
|---|---:|---|
| `VOL_ADC` | 7 | ADC1_CH6，VDD 经 R37 2M / R38 1M 分压，约 VDD/3 |
| `LIGHTADC` | 16 | ADC2_CH5，光敏二极管 |
| `CHRG` | 3 | TP4055 充电状态 |

`VOL_ADC` 反映 VDD 供电轨；USB 供电与电池供电时 VDD 的来源不同，
不能无条件把它标成独立电池电压。其出厂软件换算/校准公式未在本次分析中恢复。

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

1. GC9A01 家族面板的具体变体、完整初始化兼容性及单线读回能力。原厂驱动选择已恢复。
2. `AUD_CTRL` 的有效电平（功放使能还是静音）。
3. VDD ADC 的软件换算与校准。
4. LED 协议及显示位置映射：图中 `RGB_LED` 已明确经 R35/R36 形成 4 灯和 9 灯的并联分支，
   不是已确认的单串 13 灯链。尚未独立恢复 GPIO48 的出厂驱动配置。
