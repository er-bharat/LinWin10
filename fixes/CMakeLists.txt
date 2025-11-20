cmake_minimum_required(VERSION 3.16)
project(Win10Bar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick QuickControls2)
find_package(LayerShellQt REQUIRED)

add_executable(Win10Bar
    main.cpp
    resources.qrc
)

target_link_libraries(Win10Bar
    PRIVATE Qt6::Core
            Qt6::Gui
            Qt6::Quick
            Qt6::QuickControls2
            LayerShellQt::Interface
)

set_target_properties(Win10Bar PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
