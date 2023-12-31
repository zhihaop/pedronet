cmake_minimum_required(VERSION 3.1)
project(pedronet)

file(GLOB_RECURSE PEDRONET_SRCS src/*.cc)

if (NOT TARGET pedrolib)
    add_subdirectory(deps/pedrolib)
endif ()


add_library(pedronet SHARED ${PEDRONET_SRCS})
target_compile_features(pedronet PRIVATE cxx_std_17)
target_include_directories(pedronet PUBLIC include deps/concurrentqueue deps/nanobench/src/include)
target_link_libraries(pedronet PRIVATE pedrolib)

add_executable(test_event_loop test/test_event_loop.cc)
target_compile_features(test_event_loop PRIVATE cxx_std_17)
target_link_libraries(test_event_loop PRIVATE pedronet pedrolib)

add_executable(bench_tcp_server bench/bench_tcp_server.cc)
target_compile_features(bench_tcp_server PRIVATE cxx_std_17)
target_link_libraries(bench_tcp_server PRIVATE pedronet pedrolib)

add_executable(bench_tcp_client bench/bench_tcp_client.cc)
target_compile_features(bench_tcp_client PRIVATE cxx_std_17)
target_link_libraries(bench_tcp_client PRIVATE pedronet pedrolib)

add_executable(bench_event_queue bench/bench_event_queue.cc)
target_compile_features(bench_event_queue PRIVATE cxx_std_17)
target_link_libraries(bench_event_queue PRIVATE pedronet pedrolib)

add_executable(bench_timer_queue bench/bench_timer_queue.cc)
target_compile_features(bench_timer_queue PRIVATE cxx_std_17)
target_link_libraries(bench_timer_queue PRIVATE pedronet pedrolib)