# CMakeLists.txt - ESP-IDF Project Build Configuration for TDC7201 GPSDO Module

cmake_minimum_required(VERSION 3.16)

# Set the project name
project(tdc7201_gpsdo_module)

# Include the ESP-IDF CMake functions
include($ENV{IDF_PATH}/tools/cmake/idf.cmake)

# Add source files in the main component
set(MAIN_SRCS
    "main/main.c"
    "main/tdc7201_driver.c"
    "main/measurement.c"
    "main/wifi_config.c"
)

# Register the main component
idf_component_register(
    SRCS ${MAIN_SRCS}
    INCLUDE_DIRS "main"
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        nvs_flash
        driver
        freertos
        esp_timer
        json
)

# Set compiler options
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Werror=return-type
    -ffunction-sections
    -fdata-sections
)

# Link math library for sqrt() in measurement.c
target_link_libraries(${COMPONENT_LIB} PUBLIC m)
