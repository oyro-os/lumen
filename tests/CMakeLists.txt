# Test configuration

# Test executable
add_executable(lumen_tests
    test_main.cpp
    test_storage.cpp
    test_c_api.cpp
    test_types.cpp
    test_allocator.cpp
    test_memory_manager.cpp
    test_page.cpp
    test_buffer_pool.cpp
    test_storage_engine.cpp
    test_single_file_storage.cpp
    test_btree.cpp
    test_btree_debug.cpp
    test_btree_simple.cpp
    test_btree_insert.cpp
    test_btree_split.cpp
    test_btree_index.cpp
)

# Link libraries
target_link_libraries(lumen_tests
    lumen_static
    gtest
    gtest_main
)

# Platform-specific libraries
if(NOT WIN32)
    target_link_libraries(lumen_tests pthread)
endif()

# Test include directories
target_include_directories(lumen_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Add tests to CTest
add_test(NAME lumen_unit_tests COMMAND lumen_tests)

# Set test properties
set_tests_properties(lumen_unit_tests PROPERTIES
    TIMEOUT 300
    ENVIRONMENT "LUMEN_TEST_DIR=${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

# Create test fixtures directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)

# Performance benchmark executable (optional)
add_executable(lumen_benchmarks
    benchmark_main.cpp
)

target_link_libraries(lumen_benchmarks
    lumen_static
)

target_include_directories(lumen_benchmarks PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Benchmark is not added to CTest by default
# Run manually with: ./lumen_benchmarks

# BTreeIndex debug test executable
add_executable(test_btree_index_debug
    test_btree_index_simple_debug.cpp
)

target_link_libraries(test_btree_index_debug
    lumen_static
    gtest
    gtest_main
)

target_include_directories(test_btree_index_debug PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)