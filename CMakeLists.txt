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
    src/experience/experience_settings.cpp
)
target_include_directories(oracle_experience PUBLIC include)
target_compile_features(oracle_experience PUBLIC cxx_std_20)

add_library(oracle_input
    src/input/input_frame.cpp
)
target_include_directories(oracle_input PUBLIC include)
target_compile_features(oracle_input PUBLIC cxx_std_20)

add_library(oracle_core
    src/core/actors/actor_slot_domain.cpp
    src/core/items/item_campaign_policy.cpp
    src/core/items/item_runtime.cpp
    src/core/world/simulation_region.cpp
)
target_include_directories(oracle_core PUBLIC include)
target_compile_features(oracle_core PUBLIC cxx_std_20)
target_link_libraries(oracle_core PUBLIC oracle_experience)

add_library(oracle_content
    src/content/actors/chest_data.cpp
    src/content/actors/enemy_data.cpp
    src/content/actors/part_data.cpp
    src/content/rom/game_boy_tiles.cpp
    src/content/rom/rom_source.cpp
    src/content/rom/rom_text.cpp
    src/content/sprites/enemy_sprite.cpp
    src/content/sprites/interaction_sprite.cpp
    src/content/sprites/link_sprite.cpp
    src/content/sprites/part_sprite.cpp
    src/content/sprites/sword_sprite.cpp
    src/content/world/room_objects.cpp
    src/content/world/room_collisions.cpp
    src/content/world/room_layout.cpp
    src/content/world/room_mutations.cpp
    src/content/world/room_pixels.cpp
    src/content/world/room_tile_types.cpp
    src/content/world/room_topology.cpp
)
target_include_directories(oracle_content PUBLIC include)
target_compile_features(oracle_content PUBLIC cxx_std_20)
target_link_libraries(oracle_content PUBLIC oracle_core)

add_library(oracle_script
    src/script/runtime/campaign_script.cpp
    src/script/state/original_state.cpp
)
target_include_directories(oracle_script PUBLIC include)
target_compile_features(oracle_script PUBLIC cxx_std_20)
target_link_libraries(oracle_script PUBLIC oracle_content oracle_input)

add_library(oracle_gameplay
    src/gameplay/actors/actor_collision.cpp
    src/gameplay/actors/object_contact.cpp
    src/gameplay/actors/room_actor_loader.cpp
    src/gameplay/combat/octorok_runtime.cpp
    src/gameplay/combat/sword_runtime.cpp
    src/gameplay/interactions/interaction_target.cpp
    src/gameplay/interactions/vasu_interaction.cpp
    src/gameplay/items/chest_runtime.cpp
    src/gameplay/items/feather_runtime.cpp
    src/gameplay/player/player_hazard_runtime.cpp
    src/gameplay/player/player_traversal.cpp
)
target_include_directories(oracle_gameplay PUBLIC include)
target_compile_features(oracle_gameplay PUBLIC cxx_std_20)
target_link_libraries(
    oracle_gameplay
    PUBLIC oracle_content oracle_input oracle_script
)

add_library(oracle_presentation
    src/presentation/camera/camera.cpp
    src/presentation/camera/world_viewport.cpp
    src/presentation/render/render_plan.cpp
    src/presentation/timing/frame_timing.cpp
)
target_include_directories(oracle_presentation PUBLIC include)
target_compile_features(oracle_presentation PUBLIC cxx_std_20)
target_link_libraries(oracle_presentation PUBLIC oracle_experience)

if(MSVC)
    target_compile_options(oracle_experience PRIVATE /W4 /permissive-)
    target_compile_options(oracle_input PRIVATE /W4 /permissive-)
    target_compile_options(oracle_core PRIVATE /W4 /permissive-)
    target_compile_options(oracle_content PRIVATE /W4 /permissive-)
    target_compile_options(oracle_script PRIVATE /W4 /permissive-)
    target_compile_options(oracle_gameplay PRIVATE /W4 /permissive-)
    target_compile_options(oracle_presentation PRIVATE /W4 /permissive-)
else()
    target_compile_options(oracle_experience PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(oracle_input PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(oracle_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(oracle_content PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(oracle_script PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(oracle_gameplay PRIVATE -Wall -Wextra -Wpedantic)
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
        add_executable(oracle_room_slice apps/room_slice/main.cpp)
        target_link_libraries(
            oracle_room_slice
            PRIVATE
                oracle_content
                oracle_gameplay
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
    add_executable(
        oracle_core_tests
        tests/cpp/runtime/oracle_core_tests.cpp
    )
    target_link_libraries(
        oracle_core_tests
        PRIVATE
            oracle_core
            oracle_content
            oracle_gameplay
            oracle_presentation
    )
    if(MSVC)
        target_compile_options(oracle_core_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(oracle_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME oracle_core_tests COMMAND oracle_core_tests)
    set_tests_properties(
        oracle_core_tests
        PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()
