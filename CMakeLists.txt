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
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/verify.py $<TARGET_FILE:vrdg_test>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add test that exercises the codegen error paths in vrdg.py
add_test(NAME vrdg_codegen_errors
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_codegen_errors.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Resource lifecycle tests use a separate generated graph.
set(RESOURCE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/test_resources.h")
add_custom_command(
    OUTPUT ${RESOURCE_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/vrdg.py ${CMAKE_CURRENT_SOURCE_DIR}/test_resources.cpp > ${RESOURCE_HEADER}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vrdg.py ${CMAKE_CURRENT_SOURCE_DIR}/test_resources.cpp
    COMMENT "Generating test_resources.h using vrdg.py"
)
add_executable(vrdg_resources_test test_resources.cpp ${RESOURCE_HEADER})
target_include_directories(vrdg_resources_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME vrdg_resources COMMAND vrdg_resources_test)

set(EDGE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/test_edge_cases.h")
add_custom_command(
    OUTPUT ${EDGE_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/vrdg.py ${CMAKE_CURRENT_SOURCE_DIR}/test_edge_cases.cpp > ${EDGE_HEADER}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vrdg.py ${CMAKE_CURRENT_SOURCE_DIR}/test_edge_cases.cpp
    COMMENT "Generating test_edge_cases.h using vrdg.py"
)
add_executable(vrdg_edge_cases_test test_edge_cases.cpp ${EDGE_HEADER})
target_include_directories(vrdg_edge_cases_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME vrdg_edge_cases COMMAND vrdg_edge_cases_test)

set(ACCESS_HEADER "${CMAKE_CURRENT_BINARY_DIR}/test_access.h")
add_custom_command(
    OUTPUT ${ACCESS_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/vrdg.py ${CMAKE_CURRENT_SOURCE_DIR}/test_access.cpp > ${ACCESS_HEADER}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vrdg.py ${CMAKE_CURRENT_SOURCE_DIR}/test_access.cpp
    COMMENT "Generating test_access.h using vrdg.py"
)
add_executable(vrdg_access_test test_access.cpp ${ACCESS_HEADER})
target_include_directories(vrdg_access_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME vrdg_access COMMAND vrdg_access_test)
