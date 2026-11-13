cmake_minimum_required(VERSION 3.25)
project(security_quest LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ядро игры: правила, журнал, синхронизация. Общее для консоли и окна.
add_library(sq_core STATIC
    src/util.cpp
    src/platform.cpp
    src/ini.cpp
    src/config.cpp
    src/events.cpp
    src/game.cpp
    src/quest.cpp
    src/builtin_campaign.cpp
)
target_include_directories(sq_core PUBLIC src)
target_compile_definitions(sq_core PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(sq_core PUBLIC shell32)

add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_win32.cpp
    third_party/imgui/backends/imgui_impl_dx11.cpp
    third_party/imgui/imgui_stdlib.cpp
)
target_include_directories(imgui PUBLIC third_party/imgui third_party/imgui/backends)
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS UNICODE _UNICODE)
target_link_libraries(imgui PUBLIC d3d11 dxgi d3dcompiler dwmapi gdi32)

add_executable(sq src/main.cpp)
target_link_libraries(sq PRIVATE sq_core)

add_executable(sq-gui WIN32
    gui/main_gui.cpp
    gui/app.cpp
    gui/pages.cpp
    gui/game_view.cpp
    gui/battle.cpp
    gui/reward.cpp
    gui/cutscene.cpp
    gui/story.cpp
    gui/theme.cpp
    gui/win.cpp
    gui/resources.rc
)
target_link_libraries(sq-gui PRIVATE sq_core imgui comdlg32 ole32 uuid)

# Кампании, которые идут с программой, лежат в campaigns рядом с exe.
add_custom_command(TARGET sq-gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/campaigns $<TARGET_FILE_DIR:sq-gui>/campaigns)

foreach(target sq_core sq sq-gui)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
if(MSVC)
    target_compile_options(imgui PRIVATE /utf-8)
endif()

if(MINGW)
    # Каждый exe — один файл без DLL тулчейна; системные DLL Windows не в счёт.
    target_link_options(sq PRIVATE -static)
    target_link_options(sq-gui PRIVATE -static -municode)
endif()
