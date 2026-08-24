idf_component_register(SRCS "nucleus.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_gpio esp_wifi nvs_flash esp_netif
                                esp_event console esp_app_format spi_flash esp_timer
                                cmd_system)
