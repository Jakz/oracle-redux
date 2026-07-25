cmake_minimum_required(VERSION 3.25)

project(
    oracle_redux
    VERSION 0.1.0
    DESCRIPTION "Behavior-first C++ runtime for the Oracle campaigns"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ORACLE_BUILD_TESTS "Build headless Oracle runtime tests" ON)
option(ORACLE_BUILD_ROOM_SLICE "Build the standalone SDL3 room slice" ON)

add_library(oracle_experience
    src/experience_settings.cpp
)
target_include_directories(oracle_experience PUBLIC include)
target_compile_features(oracle_experience PUBLIC cxx_std_20)

add_library(oracle_core
    src/core/item_campaign_policy.cpp
    src/core/item_runtime.cpp
    src/core/simulation_region.cpp
)
target_include_directories(oracle_core PUBLIC include)
target_compile_features(oracle_core PUBLIC cxx_std_20)
target_link_libraries(oracle_core PUBLIC oracle_experience)

add_library(oracle_content
    src/content/rom_source.cpp
    src/content/room_collisions.cpp
    src/content/room_layout.cpp
    src/content/room_mutations.cpp
    src/content/room_pixels.cpp
)
target_include_directories(oracle_content PUBLIC include)
target_compile_features(oracle_content PUBLIC cxx_std_20)
target_link_libraries(oracle_content PUBLIC oracle_core)

add_library(oracle_presentation
    src/presentation/camera.cpp
    src/presentation/frame_timing.cpp
    src/presentation/render_plan.cpp
)
target_include_directories(oracle_presentation PUBLIC include)
target_compile_features(oracle_presentation PUBLIC cxx_std_20)
target_link_libraries(oracle_presentation PUBLIC oracle_experience)

if(MSVC)
    target_compile_options(oracle_experience PRIVATE /W4 /permissive-)
    target_compile_options(oracle_core PRIVATE /W4 /permissive-)
    target_compile_options(oracle_content PRIVATE /W4 /permissive-)
    target_compile_options(oracle_presentation PRIVATE /W4 /permissive-)
else()
    target_compile_options(oracle_experience PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(oracle_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(oracle_content PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(oracle_presentation PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ORACLE_BUILD_ROOM_SLICE)
    find_package(SDL3 3.4 QUIET CONFIG)
    if(NOT TARGET SDL3::SDL3 AND EXISTS "${CMAKE_SOURCE_DIR}/reference/SDL/CMakeLists.txt")
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(reference/SDL EXCLUDE_FROM_ALL)
    endif()

    if(TARGET SDL3::SDL3)
        add_executable(oracle_room_slice apps/room_slice_main.cpp)
        target_link_libraries(
            oracle_room_slice
            PRIVATE
                oracle_content
                oracle_presentation
                SDL3::SDL3
        )
        if(MSVC)
            target_compile_options(oracle_room_slice PRIVATE /W4 /permissive-)
        else()
            target_compile_options(
                oracle_room_slice
                PRIVATE
                    -Wall
                    -Wextra
                    -Wpedantic
            )
        endif()
    else()
        message(
            STATUS
            "SDL3 3.4+ not found; oracle_room_slice will not be built"
        )
    endif()
endif()

if(ORACLE_BUILD_TESTS)
    enable_testing()
    add_executable(oracle_core_tests tests/cpp/oracle_core_tests.cpp)
    target_link_libraries(
        oracle_core_tests
        PRIVATE
            oracle_core
            oracle_content
            oracle_presentation
    )
    if(MSVC)
        target_compile_options(oracle_core_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(oracle_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME oracle_core_tests COMMAND oracle_core_tests)
endif()
