idf_component_register(
    SRCS
        "bg95_uart.c"
        "bg95_power.c"
        "bg95_network.c"
        "bg95_mqtt.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES driver freertos
)
