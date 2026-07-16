include(CTest)

if(NOT BUILD_TESTING)
    return()
endif()

find_package(GTest CONFIG QUIET)

if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(matching_engine_core_gtest
    matching_engine_core_test.cpp
)

add_executable(line_matching_engine_gtest
    line_matching_engine_test.cpp
)

target_link_libraries(matching_engine_core_gtest
    PRIVATE
        matching_engine_core
        GTest::gtest_main
)

target_link_libraries(line_matching_engine_gtest
    PRIVATE
        matching_engine_core
        GTest::gtest_main
)

target_compile_features(matching_engine_core_gtest PRIVATE cxx_std_23)
target_compile_features(line_matching_engine_gtest PRIVATE cxx_std_23)

include(GoogleTest)
gtest_discover_tests(matching_engine_core_gtest)
gtest_discover_tests(line_matching_engine_gtest)
