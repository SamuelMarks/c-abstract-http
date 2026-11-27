cmake_minimum_required(VERSION 3.15...3.31)

include(FetchContent)

if(DEFINED FETCHCONTENT_SOURCE_DIR_GREATEST AND EXISTS "${FETCHCONTENT_SOURCE_DIR_GREATEST}/greatest.h")
    set(greatest_SOURCE_DIR "${FETCHCONTENT_SOURCE_DIR_GREATEST}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../greatest/greatest.h")
    message(STATUS "Using local greatest for tests")
    set(greatest_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../greatest")
elseif(EXISTS "/greatest/greatest.h")
    message(STATUS "Using local /greatest for tests")
    set(greatest_SOURCE_DIR "/greatest")
else()
    FetchContent_Declare(
        greatest
        GIT_REPOSITORY https://github.com/SamuelMarks/greatest.git
        GIT_TAG master
        SOURCE_SUBDIR "nonexistent_dir_to_prevent_add_subdirectory"
    )
    FetchContent_MakeAvailable(greatest)
endif()

add_library(cah_greatest INTERFACE)
target_include_directories(cah_greatest SYSTEM INTERFACE "${greatest_SOURCE_DIR}")

if(NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
    find_package(Threads REQUIRED)
endif()

if(EMSCRIPTEN)
    add_custom_target(install_ws ALL
        COMMAND npm install ws
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()

if(TARGET c-abstract-http-instrumented)
    set(C_ABSTRACT_HTTP_TEST_LINK_TARGET c-abstract-http-instrumented)
else()
    set(C_ABSTRACT_HTTP_TEST_LINK_TARGET c-abstract-http)
endif()

set(TEST_HARNESS_SOURCES
    abstract_http_test_helpers/abstract_http_helpers.c
    fuzz_sse.c
    fuzz_ws.c
)

if(C_ABSTRACT_HTTP_HEADER_ONLY OR (WIN32 AND BUILD_SHARED_LIBS) OR CMAKE_SYSTEM_NAME STREQUAL "DOS")
    set(TEST_SOURCES main.c mock_alloc.c ${TEST_HARNESS_SOURCES})
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
        list(APPEND TEST_SOURCES abstract_http_test_helpers/mock_server.c)
    endif()
    set(EXTRA_TEST_SOURCES mock_alloc.c)
else()
    set(TEST_SOURCES main.c ${TEST_HARNESS_SOURCES})
    set(EXTRA_TEST_SOURCES "")
endif()

function(cah_add_test_executable tgt_name)
    add_executable(${tgt_name} ${ARGN})

    set_target_properties(${tgt_name} PROPERTIES
        C_STANDARD 90
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )
    if(MSVC AND _CAH_MSVC_RUNTIME_LIBRARY)
        set_target_properties(${tgt_name} PROPERTIES
            MSVC_RUNTIME_LIBRARY "${_CAH_MSVC_RUNTIME_LIBRARY}"
        )
    endif()

    target_include_directories(${tgt_name} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_BINARY_DIR}"
    )

    target_link_libraries(${tgt_name} PRIVATE
        ${C_ABSTRACT_HTTP_TEST_LINK_TARGET}
        cah_greatest
    )

    if(NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
        target_link_libraries(${tgt_name} PRIVATE Threads::Threads)
    endif()

    if(WIN32)
        target_link_libraries(${tgt_name} PRIVATE ws2_32)
    endif()

    if(MSVC)
        target_compile_options(${tgt_name} PRIVATE "/W4" "/WX")
    elseif(WATCOM)
        target_compile_options(${tgt_name} PRIVATE "-wx" "-we")
        if(CMAKE_SYSTEM_NAME STREQUAL "DOS" AND NOT _CAH_DOS_16BIT)
            target_link_options(${tgt_name} PRIVATE "SHELL:option stack=524288")
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${tgt_name} PRIVATE
            "-Wall" "-Wextra" "-Werror" "-pedantic"
            "-Wconversion" "-Wsign-conversion" "-Wshadow"
            "-Wpointer-arith" "-Wdeclaration-after-statement"
            "-Wnested-externs" "-Wvla" "-Wstrict-prototypes"
            "-Wold-style-definition" "-Wformat=2" "-Wformat-security"
            "-Wundef" "-Wdouble-promotion" "-Winit-self" "-Wcast-align"
        )
    endif()

    if(C_ABSTRACT_HTTP_COVERAGE_ACTIVE)
        target_compile_options(${tgt_name} PRIVATE "--coverage")
        target_link_options(${tgt_name} PRIVATE "--coverage")
    endif()

    if(C_ABSTRACT_HTTP_HEADER_ONLY)
        target_compile_definitions(${tgt_name} PRIVATE C_ABSTRACT_HTTP_HEADER_ONLY)
        if(NOT (WIN32 AND BUILD_SHARED_LIBS) AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
            target_compile_definitions(${tgt_name} PRIVATE ${C_ABSTRACT_HTTP_MOCK_DEFS})
        endif()
    elseif(NOT (WIN32 AND BUILD_SHARED_LIBS) AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
        target_compile_definitions(${tgt_name} PRIVATE C_ABSTRACT_HTTP_TEST_OOM=1)
    endif()

    if(EMSCRIPTEN)
        target_link_options(${tgt_name} PRIVATE
            "SHELL:-s STACK_SIZE=1048576 -s ALLOW_MEMORY_GROWTH=1 -s ASYNCIFY -s EXIT_RUNTIME=1 -s ASSERTIONS=2"
        )
        add_dependencies(${tgt_name} install_ws)
    endif()

    if(WIN32 AND BUILD_SHARED_LIBS AND NOT C_ABSTRACT_HTTP_HEADER_ONLY)
        add_custom_command(TARGET ${tgt_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:c-abstract-http>
                $<TARGET_FILE_DIR:${tgt_name}>
        )
        foreach(_dep_tgt c89stringutils cfs)
            if(TARGET ${_dep_tgt})
                get_target_property(_dep_type ${_dep_tgt} TYPE)
                if(_dep_type STREQUAL "SHARED_LIBRARY")
                    add_custom_command(TARGET ${tgt_name} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            $<TARGET_FILE:${_dep_tgt}>
                            $<TARGET_FILE_DIR:${tgt_name}>
                    )
                endif()
            endif()
        endforeach()
    endif()
endfunction()

if(CMAKE_SYSTEM_NAME STREQUAL "DOS")
    set(_CAH_TEST_TIMEOUT_LONG 600)
    set(_CAH_TEST_TIMEOUT_SHORT 300)
else()
    set(_CAH_TEST_TIMEOUT_LONG 300)
    set(_CAH_TEST_TIMEOUT_SHORT 30)
endif()

cah_add_test_executable(c-abstract-http-test ${TEST_SOURCES})
add_test(NAME c-abstract-http-tests COMMAND c-abstract-http-test)
set_tests_properties(c-abstract-http-tests PROPERTIES TIMEOUT ${_CAH_TEST_TIMEOUT_LONG})
if(NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
    add_test(NAME c-abstract-http-test-worker COMMAND c-abstract-http-test --test-worker)
    set_tests_properties(c-abstract-http-test-worker PROPERTIES WILL_FAIL TRUE TIMEOUT ${_CAH_TEST_TIMEOUT_SHORT})
endif()
add_test(NAME c-abstract-http-test-arg COMMAND c-abstract-http-test -s transport_suite)
set_tests_properties(c-abstract-http-test-arg PROPERTIES TIMEOUT ${_CAH_TEST_TIMEOUT_SHORT})

if(C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS)
    cah_add_test_executable(c-abstract-http-test-ws test_ws.c ${EXTRA_TEST_SOURCES})
    add_test(NAME c-abstract-http-test-ws COMMAND c-abstract-http-test-ws)
    set_tests_properties(c-abstract-http-test-ws PROPERTIES TIMEOUT ${_CAH_TEST_TIMEOUT_SHORT})

    cah_add_test_executable(c-abstract-http-test-crypto test_crypto.c ${EXTRA_TEST_SOURCES})
    add_test(NAME c-abstract-http-test-crypto COMMAND c-abstract-http-test-crypto)
    set_tests_properties(c-abstract-http-test-crypto PROPERTIES TIMEOUT ${_CAH_TEST_TIMEOUT_SHORT})
endif()

if(C_ABSTRACT_HTTP_ENABLE_SSE)
    cah_add_test_executable(c-abstract-http-test-sse test_sse.c ${EXTRA_TEST_SOURCES})
    add_test(NAME c-abstract-http-test-sse COMMAND c-abstract-http-test-sse)
    set_tests_properties(c-abstract-http-test-sse PROPERTIES TIMEOUT ${_CAH_TEST_TIMEOUT_SHORT})
endif()

function(cah_add_fuzzer tgt_name src_file)
    add_executable(${tgt_name} "${src_file}")
    target_include_directories(${tgt_name} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_BINARY_DIR}"
    )
    target_compile_definitions(${tgt_name} PRIVATE
        C_ABSTRACT_HTTP_ENABLE_FUZZING=1
        $<$<BOOL:${C_ABSTRACT_HTTP_HEADER_ONLY}>:C_ABSTRACT_HTTP_HEADER_ONLY>
    )
    target_link_libraries(${tgt_name} PRIVATE c-abstract-http)
    target_compile_options(${tgt_name} PRIVATE -fsanitize=fuzzer)
    target_link_options(${tgt_name} PRIVATE -fsanitize=fuzzer)
    add_test(NAME c-abstract-http-${tgt_name} COMMAND ${tgt_name} -runs=1000)
endfunction()

if(C_ABSTRACT_HTTP_ENABLE_FUZZING)
    if(C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS)
        cah_add_fuzzer(fuzz_ws fuzz_ws.c)
    endif()
    if(C_ABSTRACT_HTTP_ENABLE_SSE)
        cah_add_fuzzer(fuzz_sse fuzz_sse.c)
    endif()
endif()
