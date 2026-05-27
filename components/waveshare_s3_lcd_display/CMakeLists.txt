idf_component_register(SRCS "waveshare_s3_lcd_display.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_lcd esp_lcd_gc9a01 esp_lvgl_port esp_driver_spi esp_driver_gpio esp_driver_ledc)
