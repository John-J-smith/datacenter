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

add_executable(dc_tests
    variable_test.cpp
    param_test.cpp
)
target_include_directories(dc_tests PRIVATE ${DC_PORT_DIR})
target_link_libraries(dc_tests PRIVATE dc_meter_fw dc_test_port GTest::gtest_main)

if(MINGW)
    target_link_options(dc_tests PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

include(GoogleTest)
gtest_discover_tests(dc_tests DISCOVERY_MODE PRE_TEST)
