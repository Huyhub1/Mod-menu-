cmake_minimum_required(VERSION 3.18.1)
project("imguimod")

set(CMAKE_CXX_STANDARD 14)

include_directories(
    .
    imgui
)

add_library(
    imguimod
    SHARED
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_impl_android.cpp
    imgui/imgui_impl_opengl3.cpp
    main.cpp
)

find_library(log-lib log)
find_library(android-lib android)
find_library(gles3-lib GLESv3)
find_library(egl-lib EGL)

target_link_libraries(
    imguimod
    ${log-lib}
    ${android-lib}
    ${gles3-lib}
    ${egl-lib}
)
