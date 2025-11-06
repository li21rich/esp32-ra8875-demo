idf_component_register(SRCS "controller.c" "display.c" "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES RA8875 esp_timer)
