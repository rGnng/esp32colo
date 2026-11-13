#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Transcribed from the ColoPLAY V1 MAIN and SCREEN drawings, not a board scan. */
enum {
    LCD_DC = 11,
    LCD_CS = 12,
    LCD_DATA = 13,
    LCD_CLOCK = 14,
    LCD_BACKLIGHT = 18,
    LCD_RESET = 45,
    HALF_PERIOD_US = 10,
    CAPTURE_BYTES = 8,
};

static bool bus_ready;

static void console_print(const char *format, ...) __attribute__((format(printf, 1, 2)));

static void console_print(const char *format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (length <= 0) {
        return;
    }
    size_t remaining = (size_t)length < sizeof(buffer) ? (size_t)length : sizeof(buffer) - 1;
    const char *cursor = buffer;
    while (remaining) {
        int sent = usb_serial_jtag_write_bytes(cursor, remaining, pdMS_TO_TICKS(1000));
        if (sent <= 0) {
            return;
        }
        remaining -= (size_t)sent;
        cursor += sent;
    }
}

static void print_pins(void)
{
    console_print("Reference: ColoPLAY V1 ESP32-S3; actual board must be checked.\r\n"
                  "SCLK=14 DATA=13 DC=11 CS=12 RESET=45 BACKLIGHT=18 TE=17\r\n"
                  "Main J4 -> screen J1 -> LCD J2. No separate MISO is shown.\r\n");
}

static void print_help(void)
{
    console_print("\r\nColoPLAY LCD probe -- experimental pure RAM application\r\n"
                  "help     Show commands\r\n"
                  "pins     Show the reference pinout, without driving the LCD\r\n"
                  "info     Show MCU and SDK information\r\n"
                  "probe    Read common DCS ID/status commands using SPI mode 0\r\n"
                  "probe 3  Repeat using SPI mode 3\r\n"
                  "reset    Pulse the LCD reset pin (does not reset the ESP32)\r\n"
                  "No LCD pins are configured until probe or reset is requested.\r\n"
                  "No vendor initialization or flash/eFuse writes are implemented.\r\n"
                  "A response is evidence only; this program does not select a driver.\r\n");
}

static void print_info(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    console_print("MCU model=%d cores=%u revision=%u SDK=%s free_heap=%lu\r\n",
                  (int)chip.model, (unsigned)chip.cores, (unsigned)chip.revision,
                  esp_get_idf_version(), (unsigned long)esp_get_free_heap_size());
    console_print("RAM-only build; external flash capacity is not queried here.\r\n");
}

static void prepare_bus(void)
{
    if (bus_ready) {
        return;
    }
    /* Set output latches before enabling the pads, keeping CS inactive. */
    ESP_ERROR_CHECK(gpio_set_level(LCD_CS, 1));
    ESP_ERROR_CHECK(gpio_set_level(LCD_DC, 1));
    ESP_ERROR_CHECK(gpio_set_level(LCD_CLOCK, 0));
    ESP_ERROR_CHECK(gpio_set_level(LCD_BACKLIGHT, 0));
    ESP_ERROR_CHECK(gpio_set_level(LCD_RESET, 1));
    const gpio_config_t outputs = {
        .pin_bit_mask = (1ULL << LCD_CS) | (1ULL << LCD_DC) | (1ULL << LCD_CLOCK) |
                        (1ULL << LCD_BACKLIGHT) | (1ULL << LCD_RESET),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&outputs));
    const gpio_config_t data = {
        .pin_bit_mask = 1ULL << LCD_DATA,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&data));
    /* Allow a panel held in reset at MCU startup to recover after release. */
    vTaskDelay(pdMS_TO_TICKS(150));
    bus_ready = true;
}

static void reset_panel(void)
{
    prepare_bus();
    ESP_ERROR_CHECK(gpio_set_level(LCD_CS, 1));
    ESP_ERROR_CHECK(gpio_set_level(LCD_RESET, 0));
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_ERROR_CHECK(gpio_set_level(LCD_RESET, 1));
    vTaskDelay(pdMS_TO_TICKS(150));
    console_print("LCD reset released; backlight remains off.\r\n");
}

static void write_command(uint8_t command, unsigned mode)
{
    for (int bit = 7; bit >= 0; --bit) {
        /* Both modes 0 and 3 sample on the rising edge. */
        gpio_set_level(LCD_CLOCK, 0);
        gpio_set_level(LCD_DATA, (command >> bit) & 1);
        esp_rom_delay_us(HALF_PERIOD_US);
        gpio_set_level(LCD_CLOCK, 1);
        esp_rom_delay_us(HALF_PERIOD_US);
        /* Leave the final command clock high for a contention-free turnaround. */
        if (mode == 0 && bit > 0) {
            gpio_set_level(LCD_CLOCK, 0);
        }
    }
}

static uint8_t read_byte(unsigned mode)
{
    uint8_t result = 0;
    for (unsigned bit = 0; bit < 8; ++bit) {
        gpio_set_level(LCD_CLOCK, 0);
        esp_rom_delay_us(HALF_PERIOD_US);
        gpio_set_level(LCD_CLOCK, 1);
        esp_rom_delay_us(HALF_PERIOD_US);
        result = (uint8_t)((result << 1) | gpio_get_level(LCD_DATA));
        if (mode == 0) {
            gpio_set_level(LCD_CLOCK, 0);
        }
    }
    return result;
}

static void capture(uint8_t command, unsigned mode, gpio_pull_mode_t bias,
                    bool selected, uint8_t result[CAPTURE_BYTES])
{
    gpio_set_level(LCD_CS, 1);
    gpio_set_level(LCD_CLOCK, mode == 3);
    gpio_set_level(LCD_DC, 0);
    ESP_ERROR_CHECK(gpio_set_pull_mode(LCD_DATA, GPIO_FLOATING));
    gpio_set_level(LCD_DATA, 0);
    ESP_ERROR_CHECK(gpio_set_direction(LCD_DATA, GPIO_MODE_OUTPUT));
    if (selected) {
        gpio_set_level(LCD_CS, 0);
    }
    esp_rom_delay_us(HALF_PERIOD_US);
    write_command(command, mode);

    /* Release DATA before the first falling edge of the response phase. */
    ESP_ERROR_CHECK(gpio_set_direction(LCD_DATA, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(LCD_DATA, bias));
    gpio_set_level(LCD_DC, 1);
    esp_rom_delay_us(HALF_PERIOD_US);
    for (unsigned i = 0; i < CAPTURE_BYTES; ++i) {
        result[i] = read_byte(mode);
    }
    gpio_set_level(LCD_CS, 1);
    gpio_set_level(LCD_CLOCK, mode == 3);
    ESP_ERROR_CHECK(gpio_set_pull_mode(LCD_DATA, GPIO_FLOATING));
    /* DATA remains an input between transactions. */
}

static void print_capture(const uint8_t bytes[CAPTURE_BYTES])
{
    char hex[CAPTURE_BYTES * 3 + 1];
    for (unsigned i = 0; i < CAPTURE_BYTES; ++i) {
        snprintf(hex + i * 3, 4, "%02X ", bytes[i]);
    }
    console_print("%s\r\n", hex);
}

static void probe(unsigned mode)
{
    /* Common DCS reads only. No controller-specific initialization is sent. */
    static const struct {
        uint8_t command;
        const char *name;
    } queries[] = {
        {0x04, "RDDID"}, {0x09, "RDDST"}, {0x0a, "RDDPM"},
        {0x0b, "RDDMADCTL"}, {0x0c, "RDDCOLMOD"},
        {0xda, "RDID1"}, {0xdb, "RDID2"}, {0xdc, "RDID3"},
    };
    prepare_bus();
    console_print("\r\nProbe mode=%u; clock at most 50 kHz; shared DATA=GPIO13.\r\n"
                  "Raw bytes include any dummy cycles and undriven trailing bits.\r\n",
                  mode);
    for (unsigned bias_index = 0; bias_index < 2; ++bias_index) {
        const gpio_pull_mode_t bias = bias_index ? GPIO_PULLUP_ONLY : GPIO_PULLDOWN_ONLY;
        const char *bias_name = bias_index ? "up" : "down";
        uint8_t bytes[CAPTURE_BYTES];
        console_print("\r\nWeak pull-%s; CS inactive control: ", bias_name);
        capture(0x04, mode, bias, false, bytes);
        print_capture(bytes);
        for (unsigned i = 0; i < sizeof(queries) / sizeof(queries[0]); ++i) {
            for (unsigned repeat = 1; repeat <= 2; ++repeat) {
                capture(queries[i].command, mode, bias, true, bytes);
                console_print("CS=active pull=%s cmd=%02X %-9s repeat=%u: ",
                              bias_name, queries[i].command, queries[i].name, repeat);
                print_capture(bytes);
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
    }
    console_print("\r\nProbe complete. Compare repeated response bits across both biases\r\n"
                  "and against the CS-inactive controls. Trailing floating bits may vary.\r\n"
                  "All 00/FF or bias-following results are inconclusive, not an LCD ID.\r\n"
                  "Even a repeatable ID may be shared by multiple controller models.\r\n");
}

static void execute(char *line)
{
    while (isspace((unsigned char)*line)) {
        ++line;
    }
    size_t length = strlen(line);
    while (length && isspace((unsigned char)line[length - 1])) {
        line[--length] = '\0';
    }
    if (!strcmp(line, "help") || !length) {
        print_help();
    } else if (!strcmp(line, "pins")) {
        print_pins();
    } else if (!strcmp(line, "info")) {
        print_info();
    } else if (!strcmp(line, "probe") || !strcmp(line, "probe 0")) {
        probe(0);
    } else if (!strcmp(line, "probe 3")) {
        probe(3);
    } else if (!strcmp(line, "reset")) {
        reset_panel();
    } else {
        console_print("Unknown command; type help.\r\n");
    }
}

void app_main(void)
{
    usb_serial_jtag_driver_config_t usb = {
        .tx_buffer_size = 4096,
        .rx_buffer_size = 256,
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    vTaskDelay(pdMS_TO_TICKS(500));
    print_help();
    console_print("\r\nprobe> ");

    char line[64];
    size_t used = 0;
    bool overflow = false;
    bool previous_cr = false;
    for (;;) {
        char ch;
        if (usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(100)) != 1) {
            continue;
        }
        if (ch == '\n' && previous_cr) {
            previous_cr = false;
            continue;
        }
        previous_cr = ch == '\r';
        if (ch == '\r' || ch == '\n') {
            console_print("\r\n");
            if (overflow) {
                console_print("Command too long; discarded.\r\n");
            } else {
                line[used] = '\0';
                execute(line);
            }
            used = 0;
            overflow = false;
            console_print("\r\nprobe> ");
        } else if (!overflow && (ch == '\b' || ch == 127)) {
            if (used) {
                --used;
                console_print("\b \b");
            }
        } else if (isprint((unsigned char)ch) || ch == '\t') {
            if (used < sizeof(line) - 1 && !overflow) {
                line[used++] = ch;
                console_print("%c", ch);
            } else {
                overflow = true;
            }
        }
    }
}
