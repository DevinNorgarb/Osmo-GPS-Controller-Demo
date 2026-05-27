if(NOT CONFIG_LILYGO_LILY_PI)
    return()
endif()

idf_component_register(SRCS "lilypi_display.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_lcd esp_lcd_st7796 esp_lvgl_port esp_driver_spi esp_driver_gpio)
