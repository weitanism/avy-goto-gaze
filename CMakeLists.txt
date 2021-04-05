cmake_minimum_required(VERSION 3.19)
project(tobii-jump)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

find_package(X11 REQUIRED)
find_package(Cairo REQUIRED)

add_library(gaze SHARED
    src/use_gaze_mod.cxx)
target_link_libraries(gaze
    /usr/lib/tobii/libtobii_stream_engine.so
    pthread)

add_executable(overlay
    src/overlay.cxx)
target_include_directories(overlay PRIVATE
    ${CAIRO_INCLUDE_DIRS})
target_link_libraries(overlay
    gaze
    X11::X11
    X11::Xext
    X11::Xfixes
    ${CAIRO_LIBRARIES}
    pthread)

add_executable(test
    src/main.cxx)
target_link_libraries(test
    gaze
    pthread)
