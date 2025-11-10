idf_component_register(SRCS "main.c"
                    REQUIRES esp_wifi esp_eth nvs_flash esp_netif esp_http_server i2c_bus esp_pm esp_timer 
                    INCLUDE_DIRS ".")

