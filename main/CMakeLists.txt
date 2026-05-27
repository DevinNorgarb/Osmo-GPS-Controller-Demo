set(MAIN_SRCS
    "app_main.c"
    "../utils/crc/custom_crc16.c"
    "../utils/crc/custom_crc32.c"
    "../protocol/dji_protocol_parser.c"
    "../protocol/dji_protocol_data_processor.c"
    "../protocol/dji_protocol_data_descriptors.c"
    "../protocol/dji_protocol_data_structures.c"
    "../ble/ble.c"
    "../data/data.c"
    "../logic/connect_logic.c"
    "../logic/command_logic.c"
    "../logic/gps_logic.c"
    "../logic/status_logic.c"
    "../logic/enums_logic.c"
    "../logic/key_logic.c"
    "../logic/light_logic.c"
    "../test/test_gps.c"
)

set(MAIN_PRIV_REQUIRES bt nvs_flash esp_driver_uart esp_driver_gpio led_strip)
set(MAIN_INCLUDE_DIRS "." "../utils/crc" "../protocol" "../ble" "../data" "../logic" "../test")

list(APPEND MAIN_SRCS "../logic/display_logic.c")

# Kconfig CONFIG_* is reliable under idf.py; PlatformIO may configure main before sdkconfig is merged.
if(CONFIG_LILYGO_LILY_PI)
    list(APPEND MAIN_PRIV_REQUIRES lilypi_display esp_lvgl_port lvgl)
endif()

if(CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128)
    list(APPEND MAIN_PRIV_REQUIRES waveshare_s3_lcd_display esp_lvgl_port lvgl)
endif()

if(BUILD_WITH_PLATFORMIO)
    macro(_main_append_display_deps_from_sdkconfig path)
        if(EXISTS "${path}")
            file(READ "${path}" _sdkconfig_txt)
            if(_sdkconfig_txt MATCHES "CONFIG_LILYGO_LILY_PI=y")
                list(APPEND MAIN_PRIV_REQUIRES lilypi_display esp_lvgl_port lvgl)
                list(APPEND MAIN_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/components/lilypi_display/include")
            endif()
            if(_sdkconfig_txt MATCHES "CONFIG_WAVESHARE_ESP32_S3_TOUCH_LCD_128=y")
                list(APPEND MAIN_PRIV_REQUIRES waveshare_s3_lcd_display esp_lvgl_port lvgl)
                list(APPEND MAIN_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/components/waveshare_s3_lcd_display/include")
            endif()
        endif()
    endmacro()

    _main_append_display_deps_from_sdkconfig("${CMAKE_SOURCE_DIR}/sdkconfig")
    _main_append_display_deps_from_sdkconfig("${CMAKE_SOURCE_DIR}/sdkconfig.lilygo-lily-pi")
    _main_append_display_deps_from_sdkconfig("${CMAKE_SOURCE_DIR}/sdkconfig.waveshare-esp32-s3-touch-lcd-1")
endif()

idf_component_register(SRCS ${MAIN_SRCS}
                    PRIV_REQUIRES ${MAIN_PRIV_REQUIRES}
                    INCLUDE_DIRS ${MAIN_INCLUDE_DIRS})
