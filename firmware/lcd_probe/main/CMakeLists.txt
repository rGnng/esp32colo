idf_component_register(
    SRCS "probe_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_gpio esp_driver_usb_serial_jtag esp_hw_support esp_system esp_rom
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
