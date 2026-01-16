# CMakeLists.txt for logging component

# Get git short SHA for build-time injection
execute_process(
    COMMAND git rev-parse --short=8 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHORT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Default to "00000000" if git command fails
if(NOT GIT_SHORT_SHA)
    set(GIT_SHORT_SHA "00000000")
endif()

idf_component_register(
    SRCS "src/session_helper.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash spi_flash
)

# Inject GIT_SHORT_SHA as a compile definition
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    GIT_SHORT_SHA="${GIT_SHORT_SHA}"
)
