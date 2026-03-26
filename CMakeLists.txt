cmake_minimum_required(VERSION 3.28)
project(gospel)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.x
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    cli11_proj
    QUIET
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.6.2
)
FetchContent_MakeAvailable(cli11_proj)

find_package(witch_math CONFIG REQUIRED)
find_package(pattern_scanner CONFIG REQUIRED)
find_package(xorstr CONFIG REQUIRED)

if (NOT TARGET hello_imgui)
    message(STATUS "Fetching hello_imgui")
    include(FetchContent)
    FetchContent_Declare(hello_imgui GIT_REPOSITORY https://github.com/pthom/hello_imgui.git GIT_TAG master EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(hello_imgui)
endif()

# 3/  Option 3: via vcpkg
# i/ You can install hello_imgui via vcpkg with:
#     vcpkg install "hello-imgui[opengl3-binding,glfw-binding]"
# ii/ Then you can use it inside CMake with:
#     find_package(hello-imgui CONFIG REQUIRED)


# Build your app
# ==============
# hello_imgui_add_app is a helper function, similar to cmake's "add_executable"
# Usage:
# hello_imgui_add_app(app_name file1.cpp file2.cpp ...)
#
# Features:
# * It will automatically link the target to the required libraries (hello_imgui, OpenGl, glad, etc)
# * It will embed the assets (for desktop, mobile, and emscripten apps)
# * It will perform additional customization (app icon and name on mobile platforms, etc)

# Now you can build your app with
#     mkdir build && cd build && cmake .. && cmake --build .

# By default, we install in a portable way (i.e. assets and executable are in the same folder)
option(EXAMPLE_INTEGRATION_PORTABLE_INSTALL "Install in a portable way" ON)


# portable installation is the easiest way to install the app
hello_imgui_add_app(gospel main.cpp  imgui_impl_main.cpp imgui_impl_win32.cpp imgui_impl_dx11.cpp)

target_compile_features(gospel PRIVATE cxx_std_23)
target_link_libraries(gospel PRIVATE spdlog::spdlog)
target_link_libraries(gospel PRIVATE d3d11)
target_link_libraries(gospel PRIVATE witch_cult::witch_math)
target_link_libraries(gospel PRIVATE witch_cult::pattern_scanner)
target_link_libraries(gospel PRIVATE witch_cult::xorstr)
target_link_libraries(gospel PRIVATE CLI11::CLI11)