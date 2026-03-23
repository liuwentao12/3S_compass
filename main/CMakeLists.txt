idf_component_register(SRCS "main.c"
                            "interaction/audio.c"
                            "interaction/tft_display.c"
                            "Navigation/sensor_modules.c"
                            "Navigation/navigator_logic.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_i2s nvs_flash esp_ringbuf esp_driver_dac esp_driver_gpio esp_driver_i2c esp_adc bt esp_lcd
                       )
