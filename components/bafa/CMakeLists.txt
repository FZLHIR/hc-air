idf_component_register(SRCS "bafa.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_timer esp_wifi lwip nvs_flash
                    data )
