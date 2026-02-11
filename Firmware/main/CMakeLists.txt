idf_component_register(
    SRCS "Main.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs driver  # Added fatfs for official SD driver
)
