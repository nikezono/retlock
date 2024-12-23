cmake_minimum_required(VERSION 3.14...3.22)

project(ReTLockExample LANGUAGES CXX)

add_executable(${PROJECT_NAME} example.cpp)
target_link_libraries(${PROJECT_NAME} ReTLock::ReTLock)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)