idf_component_register(SRCS "main.c"
                            "interaction/audio.c"
                            "interaction/tft_display.c"
                            "interaction/voice_assistant.c"
                            "interaction/ble_server.c"
                            "Navigation/sensor_modules.c"
                            "Navigation/navigator_logic.c"
                            "storage/sd_card.c"
                            "storage/track_logger.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_i2s nvs_flash esp_ringbuf esp_driver_dac esp_driver_gpio esp_driver_i2c esp_adc bt esp_lcd fatfs sdmmc esp_driver_sdmmc esp-sr app_update spi_flash
                       )
