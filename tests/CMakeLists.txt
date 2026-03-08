cmake_minimum_required(VERSION 3.10)

# Download greatest.h
include(FetchContent)
file(DOWNLOAD "https://raw.githubusercontent.com/SamuelMarks/greatest/cmake-and-msvc/greatest.h"
     "${CMAKE_CURRENT_BINARY_DIR}/greatest.h")

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(../include)
include_directories(../src)

# Need to build cdd_test_helpers/mock_server.c as part of the test executable
set(TEST_SOURCES
    main.c
    cdd_test_helpers/mock_server.c
)

add_executable(c-abstract-http-test ${TEST_SOURCES})
target_link_libraries(c-abstract-http-test PRIVATE c-abstract-http)

if(WIN32)
    target_link_libraries(c-abstract-http-test PRIVATE ws2_32)
endif()

add_test(NAME c-abstract-http-tests COMMAND c-abstract-http-test)
