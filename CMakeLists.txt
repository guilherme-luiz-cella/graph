cmake_minimum_required(VERSION 3.20)

# CMake 4+ refuses subprojects pinned to cmake_minimum_required < 3.5.
# glm/assimp still ship with old floors. Force a policy floor for all subprojects.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

project(cg_final LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# --- GLFW ---
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# --- GLM ---
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

# --- ASSIMP ---
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)

# --- GLAD (generated, GL 3.3 core) ---
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.6
    SOURCE_SUBDIR cmake
)
FetchContent_MakeAvailable(glad)
glad_add_library(glad STATIC API gl:core=3.3)

# --- stb + miniaudio (single-header, no build) ---
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE vendor/stb)

add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE vendor/miniaudio)

# --- App ---
add_executable(cg_final
    src/main.cpp
    src/shader.cpp
    src/camera.cpp
    src/model.cpp
    src/skybox.cpp
    src/primitives.cpp
    src/animated_model.cpp
    src/animator.cpp
    src/procgen.cpp
)

target_include_directories(cg_final PRIVATE src)
target_link_libraries(cg_final PRIVATE glfw glad glm::glm assimp stb miniaudio)

# Copy shaders + assets next to binary
add_custom_command(TARGET cg_final POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:cg_final>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:cg_final>/assets
)

if(APPLE)
    target_link_libraries(cg_final PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()
