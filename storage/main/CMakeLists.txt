idf_component_register(SRCS "main.cpp" "LSMextension.cpp"
                    REQUIRES esp_wifi esp_eth nvs_flash esp_netif esp_http_server esp_pm esp_timer 
                    PRIV_REQUIRES LSM6DSV16X
                    INCLUDE_DIRS ".")

