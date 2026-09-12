idf_component_register(SRCS "cirvane.c" "cirvane_os.c"
                            "cirvane_ota_policy.c" "cirvane_ota_remote.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_gpio esp_wifi nvs_flash esp_netif
                                esp_event console esp_app_format spi_flash app_update
                                bootloader_support cmd_system esp_http_client
                                mbedtls)
