idf_component_register(SRCS "main.cpp"
                    REQUIRES esp_wifi esp_eth nvs_flash esp_netif esp_http_server esp_pm esp_timer 
                    INCLUDE_DIRS ".")

