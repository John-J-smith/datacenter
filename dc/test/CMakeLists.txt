include(FetchContent)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_library(dc_test_port STATIC port/dc_storage_sim.c)
target_include_directories(dc_test_port PUBLIC
    ${DC_PORT_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(datacenter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(datacenter PRIVATE DC_TEST)

add_executable(dc_tests
    variable_layout_test.cpp
    variable_rw_test.cpp
    variable_recovery_test.cpp
    variable_backup_test.cpp
    param_rw_test.cpp
    param_layout_test.cpp
)
target_include_directories(dc_tests PRIVATE ${DC_PORT_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(dc_tests PRIVATE DC_TEST)
target_link_libraries(dc_tests PRIVATE datacenter dc_test_port GTest::gtest_main)

if(MINGW)
    target_link_options(dc_tests PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

include(GoogleTest)
gtest_discover_tests(dc_tests DISCOVERY_MODE PRE_TEST)
