set(COMPONENT_SRCS "")

file(GLOB MAIN_APP "*.c" "*.cpp")
list(APPEND COMPONENT_SRCS ${MAIN_APP})

file(GLOB SRC_A_FILES "status_led_manager/*.c" "status_led_manager/*.cpp")
list(APPEND COMPONENT_SRCS ${SRC_A_FILES})

file(GLOB SRC_B_FILES "ambient_light_manager/*.c" "ambient_light_manager/*.cpp" "ambient_light_manager/VEML7700/*.c" "ambient_light_manager/VEML7700/*.cpp")
list(APPEND COMPONENT_SRCS ${SRC_B_FILES})

# Register the component with ESP-IDF
idf_component_register(
    SRCS ${COMPONENT_SRCS}
    INCLUDE_DIRS "." "status_led_manager" "ambient_light_manager" # Add all necessary include directories
    # REQUIRES other_component_name # Add any required components here
)