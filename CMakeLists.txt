cmake_minimum_required(VERSION 3.16)
project(iu_uring_tutorial C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}" ${CMAKE_MODULE_PATH})
set(CMAKE_C_STANDARD 99)

find_package(LibUring)
list(APPEND FOLLY_LINK_LIBRARIES ${LIBURING_LIBRARIES})
list(APPEND FOLLY_INCLUDE_DIRECTORIES ${LIBURING_INCLUDE_DIRS})

add_executable(step1 step1.c)
add_executable(step1_uring step1_uring.c)
add_executable(step1_liburing step1_liburing.c)
add_executable(step2 step2.c)
add_executable(step3 step3.c)
add_executable(io_uring_test io_uring_test.c)
add_executable(io_uring_test_2 io_uring_test_2.c)

target_include_directories(io_uring_test PUBLIC "/usr/src/linux-headers-5.7.0-050700rc6")
target_include_directories(io_uring_test_2 PUBLIC "/usr/src/linux-headers-5.7.0-050700rc6")

target_link_libraries(step1_liburing ${LIBURING_LIBRARIES})
target_link_libraries(step3 ${LIBURING_LIBRARIES})
target_link_libraries(step3 ${LIBURING_LIBRARIES})
target_link_libraries(io_uring_test ${LIBURING_LIBRARIES})
target_link_libraries(io_uring_test_2 ${LIBURING_LIBRARIES})
