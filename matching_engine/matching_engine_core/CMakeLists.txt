set(TYPE STATIC)
set(STD c++23)
set(VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    abseil
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG 20250127.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(abseil)

super_module()

target_link_libraries(matching_engine_core PUBLIC absl::flat_hash_map)
