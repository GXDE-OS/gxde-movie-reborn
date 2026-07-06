project(libgxmr_test)

set(CMAKE_AUTOMOC ON)
set(CMD_NAME gxmr_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-fpermissive -Wno-error")

include_directories(${CMAKE_INCLUDE_CURRENT_DIR})

set(SRCS gxmr_test.cpp)

add_executable(${CMD_NAME} ${SRCS})
target_include_directories(${CMD_NAME} PUBLIC 
    ${PROJECT_SOURCE_DIR}/../libgxmr-qt6
    ${PROJECT_SOURCE_DIR})

target_link_libraries(${CMD_NAME} Qt6::Widgets gxmr-qt6)
