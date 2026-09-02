cmake_minimum_required(VERSION 3.16)
project(cs_g29_reader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
find_package(Threads REQUIRED)

add_executable(g29_reader
  src/main.cc
  src/mapping.cc
  src/evdev_reader.cc
  src/mock_reader.cc
  src/command_serializer.cc
  src/mqtt_publisher.cc
  src/device_scan.cc
)

target_include_directories(g29_reader PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${LIBEVDEV_INCLUDE_DIRS}
  ${MOSQUITTO_INCLUDE_DIRS}
)

target_link_libraries(g29_reader PRIVATE
  ${LIBEVDEV_LIBRARIES}
  ${MOSQUITTO_LIBRARIES}
  Threads::Threads
)
target_compile_options(g29_reader PRIVATE
  ${LIBEVDEV_CFLAGS_OTHER}
  ${MOSQUITTO_CFLAGS_OTHER}
)

install(TARGETS g29_reader RUNTIME DESTINATION bin)
install(FILES config/g29_mapping.conf DESTINATION share/cs_g29_reader)
