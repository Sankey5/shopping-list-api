cmake_minimum_required(VERSION 3.2.0)

set(PROJECT_NAME shoppingList)
project(${PROJECT_NAME} LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

add_subdirectory(dependencies/cpp_redis)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/crow/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/boost/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/cpp_redis/includes)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} boost_system)
target_link_libraries(${PROJECT_NAME} cpp_redis)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
