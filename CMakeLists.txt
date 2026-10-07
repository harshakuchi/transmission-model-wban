cmake_minimum_required(VERSION 3.20)
project(wban_anomaly_low_energy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(NS3_ROOT "/home/harsha/ns-3-dev" CACHE PATH "Existing ns-3 source/build tree")
set(NS3_BUILD_DIR "${NS3_ROOT}/build")
if(NOT EXISTS "${NS3_BUILD_DIR}/include/ns3/lr-wpan-helper.h")
  message(FATAL_ERROR "NS-3 LR-WPAN headers not found under ${NS3_BUILD_DIR}; set -DNS3_ROOT=<ns-3 tree>")
endif()

add_executable(wban-anomaly-low-energy scratch/wban-anomaly-low-energy.cc)
target_include_directories(wban-anomaly-low-energy PRIVATE "${NS3_BUILD_DIR}/include")
target_compile_definitions(wban-anomaly-low-energy PRIVATE __LINUX__ NS3_LOG_ENABLE NS3_ASSERT_ENABLE)
target_link_directories(wban-anomaly-low-energy PRIVATE "${NS3_BUILD_DIR}/lib")
target_link_options(wban-anomaly-low-energy PRIVATE "-Wl,-rpath,${NS3_BUILD_DIR}/lib")
target_link_libraries(wban-anomaly-low-energy PRIVATE
  ns3-dev-lr-wpan-default ns3-dev-spectrum-default ns3-dev-propagation-default
  ns3-dev-mobility-default ns3-dev-antenna-default ns3-dev-energy-default
  ns3-dev-network-default ns3-dev-stats-default ns3-dev-core-default
  gsl gslcblas sqlite3 stdc++_libbacktrace)
