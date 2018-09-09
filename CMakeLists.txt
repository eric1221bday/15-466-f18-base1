cmake_minimum_required(VERSION 3.2)

project(walking-simulator)

set(CMAKE_CXX_STANDARD 14)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm REQUIRED)

set(MAIN_FILES main.cpp
        data_path.cpp
        compile_program.cpp
        vertex_color_program.cpp
        Scene.cpp
        Mode.cpp
        MenuMode.cpp
        Load.cpp
        MeshBuffer.cpp
        draw_text.cpp
        PhoneBankMode.cpp
        Sound.cpp)

add_executable(walking-simulator ${MAIN_FILES})

target_include_directories(walking-simulator PUBLIC ${OPENGL_INCLUDE_DIR} ${SDL2_INCLUDE_DIRS})

target_link_libraries(walking-simulator ${OPENGL_LIBRARIES} ${SDL2_LIBRARIES})