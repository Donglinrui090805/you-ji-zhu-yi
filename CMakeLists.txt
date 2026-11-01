cmake_minimum_required(VERSION 3.16)
project(storygame)
set(CMAKE_CXX_STANDARD 17)
add_compile_options(/utf-8)

# Use the locally built OpenCV (opencv_world481)
set(OpenCV_DIR "D:/src/storygame/x64/vc16/lib")
find_package(OpenCV REQUIRED)

add_executable(storygame youjizhuyi.cpp)
target_link_libraries(storygame ${OpenCV_LIBS} gdiplus)

# Copy DLLs next to the exe
add_custom_command(TARGET storygame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/opencv_world481.dll"
    $<TARGET_FILE_DIR:storygame>)
