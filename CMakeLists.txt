cmake_minimum_required(VERSION 3.10)
project(vrdg_test)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Python
find_package(Python3 REQUIRED)

# Custom command to run the Python generator
set(GENERATED_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/test.h")
set(SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/test.cpp")

add_custom_command(
    OUTPUT ${GENERATED_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/vrdg.py ${SOURCE_FILE} > ${GENERATED_HEADER}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vrdg.py ${SOURCE_FILE}
    COMMENT "Generating test.h from test.cpp using vrdg.py"
)

# Create custom target for the generated header
add_custom_target(generate_header ALL DEPENDS ${GENERATED_HEADER})

# Main executable
add_executable(vrdg_test test.cpp)

# Ensure the header is generated before building the executable
add_dependencies(vrdg_test generate_header)

# Include directories
target_include_directories(vrdg_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Enable testing
enable_testing()

# Add test that runs verify.py
add_test(NAME vrdg_verify
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/verify.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add test that exercises the codegen error paths in vrdg.py
add_test(NAME vrdg_codegen_errors
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_codegen_errors.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
