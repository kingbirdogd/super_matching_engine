add_executable(flat_hash_map_latency_benchmark
    flat_hash_map_latency_benchmark.cpp
)

target_link_libraries(flat_hash_map_latency_benchmark
    PRIVATE
        absl::flat_hash_map
)

target_compile_features(flat_hash_map_latency_benchmark PRIVATE cxx_std_23)
target_compile_options(flat_hash_map_latency_benchmark PRIVATE ${SUPER_COMPILE_FLAGS})
