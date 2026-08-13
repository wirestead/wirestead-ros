# Copyright 2026 Jinwoo Sung
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)
project(wirestead_ros VERSION 0.1.0 LANGUAGES CXX)

find_package(ament_cmake REQUIRED)
find_package(wirestead CONFIG REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(diagnostic_updater REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(sensor_msgs REQUIRED)

add_library(${PROJECT_NAME} SHARED src/callback_gate.cpp src/diagnostics.cpp)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
target_include_directories(
  ${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(
  ${PROJECT_NAME}
  PUBLIC
    wirestead::wirestead
    ${diagnostic_msgs_TARGETS}
    diagnostic_updater::diagnostic_updater
)
target_compile_definitions(${PROJECT_NAME} PRIVATE WIRESTEAD_ROS_BUILDING_LIBRARY)

# Reference driver for third-party authors. Built so it cannot rot, installed
# so it can actually be run against a device.
add_executable(serial_line_driver examples/serial_line_driver.cpp)
target_link_libraries(
  serial_line_driver
  PRIVATE
    ${PROJECT_NAME}
    ${rclcpp_TARGETS}
    ${rclcpp_lifecycle_TARGETS}
    ${sensor_msgs_TARGETS}
)

install(TARGETS serial_line_driver DESTINATION lib/${PROJECT_NAME})

install(
  DIRECTORY include/
  DESTINATION include
)
install(
  TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  ament_add_gtest(test_callback_gate test/test_callback_gate.cpp)
  target_link_libraries(test_callback_gate ${PROJECT_NAME})

  ament_add_gtest(test_diagnostics test/test_diagnostics.cpp)
  target_link_libraries(test_diagnostics ${PROJECT_NAME})
endif()

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(wirestead diagnostic_msgs diagnostic_updater)
ament_package()
