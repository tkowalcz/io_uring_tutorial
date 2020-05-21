cmake_minimum_required(VERSION 3.16)
project(iu_uring_tutorial C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}" ${CMAKE_MODULE_PATH})
set(CMAKE_C_STANDARD 99)

find_package(LibUring)
list(APPEND FOLLY_LINK_LIBRARIES ${LIBURING_LIBRARIES})
list(APPEND FOLLY_INCLUDE_DIRECTORIES ${LIBURING_INCLUDE_DIRS})

add_executable(example_1_manual example_1_manual.c)
add_executable(example_2_liburing example_2_liburing.c)

target_include_directories(example_1_manual PUBLIC "/usr/src/linux-headers-5.7.0-050700rc6")
target_include_directories(example_2_liburing PUBLIC "/usr/src/linux-headers-5.7.0-050700rc6")

target_link_libraries(example_1_manual ${LIBURING_LIBRARIES})
target_link_libraries(example_2_liburing ${LIBURING_LIBRARIES})
