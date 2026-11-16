cmake_minimum_required(VERSION 3.10)

# googletest, discord-rpc and several submodules still declare minimum
# versions below 3.5, which CMake 4 refuses to configure. Treat them as 3.5
# unless CMAKE_POLICY_VERSION_MINIMUM was given on the command line or in the
# environment.
if (NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM AND NOT DEFINED ENV{CMAKE_POLICY_VERSION_MINIMUM})
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

include(CMakeDependentOption)

option(DTV "Enable video output" OFF)
option(PIRANHA_ENABLED "Enable scripting input" ON)
option(DISCORD_ENABLED "Enable Discord Rich Presence" ON)

# Default to compiling as Release in single-configuration generators
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

if (DTV)
    add_compile_definitions(ATG_ENGINE_SIM_VIDEO_CAPTURE)
endif (DTV)

if (PIRANHA_ENABLED)
    add_compile_definitions(ATG_ENGINE_SIM_PIRANHA_ENABLED)
endif (PIRANHA_ENABLED)

if (DISCORD_ENABLED)
    add_compile_definitions(ATG_ENGINE_SIM_DISCORD_ENABLED)
endif (DISCORD_ENABLED)

# Enable group projects in folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake")

project(engine-sim VERSION 0.1.12)

set(CMAKE_CXX_STANDARD 17)

# ========================================================
# GTEST

include(FetchContent)
FetchContent_Declare(
    googletest
    URL
    https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set_property(TARGET gmock PROPERTY FOLDER "gtest")
set_property(TARGET gmock_main PROPERTY FOLDER "gtest")
set_property(TARGET gtest PROPERTY FOLDER "gtest")
set_property(TARGET gtest_main PROPERTY FOLDER "gtest")

# ========================================================

add_library(engine-sim STATIC
    # Source files
    src/audio_buffer.cpp
    src/camshaft.cpp
    src/crankshaft.cpp
    src/combustion_chamber.cpp
    src/connecting_rod.cpp
    src/convolution_filter.cpp
    src/cylinder_bank.cpp
    src/cylinder_head.cpp
    src/delay_filter.cpp
    src/derivative_filter.cpp
    src/direct_throttle_linkage.cpp
    src/dynamometer.cpp
    src/engine.cpp
    src/engine_list.cpp
    src/exhaust_system.cpp
    src/feedback_comb_filter.cpp
    src/filter.cpp
    src/fuel.cpp
    src/function.cpp
    src/gas_system.cpp
    src/gaussian_filter.cpp
    src/governor.cpp
    src/ignition_module.cpp
    src/impulse_response.cpp
    src/intake.cpp
    src/compressor.cpp
    src/jitter_filter.cpp
    src/leveling_filter.cpp
    src/low_pass_filter.cpp
    src/part.cpp
    src/piston.cpp
    src/piston_engine_simulator.cpp
    src/simulator.cpp
    src/spoken_text.cpp
    src/standard_valvetrain.cpp
    src/starter_motor.cpp
    src/synthesizer.cpp
    src/throttle.cpp
    src/transmission.cpp
    src/utilities.cpp
    src/valvetrain.cpp
    src/vehicle.cpp
    src/vehicle_drag_constraint.cpp
    src/vtec_valvetrain.cpp

    # Include files
    include/audio_buffer.h
    include/application_settings.h
    include/camshaft.h
    include/crankshaft.h
    include/combustion_chamber.h
    include/connecting_rod.h
    include/convolution_filter.h
    include/cylinder_bank.h
    include/cylinder_head.h
    include/delay_filter.h
    include/derivative_filter.h
    include/direct_throttle_linkage.h
    include/dynamometer.h
    include/engine.h
    include/exhaust_system.h
    include/feedback_comb_filter.h
    include/filter.h
    include/fuel.h
    include/function.h
    include/gas_system.h
    include/gaussian_filter.h
    include/governor.h
    include/ignition_module.h
    include/impulse_response.h
    include/intake.h
    include/compressor.h
    include/jitter_filter.h
    include/leveling_filter.h
    include/low_pass_filter.h
    include/part.h
    include/piston.h
    include/piston_engine_simulator.h
    include/simulator.h
    include/engine_list.h
    include/spoken_text.h
    include/standard_valvetrain.h
    include/starter_motor.h
    include/synthesizer.h
    include/throttle.h
    include/transmission.h
    include/units.h
    include/utilities.h
    include/valvetrain.h
    include/vehicle.h
    include/vehicle_drag_constraint.h
    include/vtec_valvetrain.h
)

target_link_libraries(engine-sim
    simple-2d-constraint-solver
    csv-io
    delta-basic)

target_include_directories(engine-sim
    PUBLIC dependencies/submodules)

if (PIRANHA_ENABLED)
    add_library(engine-sim-script-interpreter STATIC
        # Source files
        scripting/src/channel_types.cpp
        scripting/src/compiler.cpp
        scripting/src/engine_context.cpp
        scripting/src/language_rules.cpp

        # Include files
        scripting/include/actions.h
        scripting/include/camshaft_node.h
        scripting/include/compressor_node.h
        scripting/include/channel_types.h
        scripting/include/compiler.h
        scripting/include/connecting_rod_node.h
        scripting/include/crankshaft_node.h
        scripting/include/cylinder_bank_node.h
        scripting/include/cylinder_head_node.h
        scripting/include/engine_context.h
        scripting/include/engine_node.h
        scripting/include/engine_sim.h
        scripting/include/exhaust_system_node.h
        scripting/include/intake_node.h
        scripting/include/function_node.h
        scripting/include/ignition_module_node.h
        scripting/include/ignition_wire_node.h
        scripting/include/impulse_response_node.h
        scripting/include/intake_node.h
        scripting/include/language_rules.h
        scripting/include/node.h
        scripting/include/object_reference_node.h
        scripting/include/object_reference_node_output.h
        scripting/include/piranha.h
        scripting/include/piston_node.h
        scripting/include/rod_journal_node.h
        scripting/include/standard_valvetrain_node.h
        scripting/include/transmission_node.h
        scripting/include/valvetrain_node.h
        scripting/include/vtec_valvetrain_node.h
		scripting/include/vehicle_node.h
    )

    target_include_directories(engine-sim-script-interpreter
        PUBLIC dependencies/submodules)

    # The interpreter builds engine-sim objects, so it links engine-sim: GNU ld
    # then puts engine-sim after it on every command line that uses both
    target_link_libraries(engine-sim-script-interpreter
        csv-io
        piranha
        engine-sim)
endif (PIRANHA_ENABLED)

add_executable(engine-sim-app WIN32
    # Source files
    src/main.cpp
    src/engine_sim_application.cpp
    src/geometry_generator.cpp
    src/simulation_object.cpp
    src/piston_object.cpp
    src/connecting_rod_object.cpp
    src/ui_element.cpp
    src/ui_manager.cpp
    src/cylinder_pressure_gauge.cpp
    src/ui_math.cpp
    src/gauge.cpp
    src/crankshaft_object.cpp
    src/cylinder_bank_object.cpp
    src/cylinder_head_object.cpp
    src/ui_button.cpp
    src/ui_utilities.cpp
    src/combustion_chamber_object.cpp
    src/oscilloscope.cpp
    src/shaders.cpp
    src/engine_view.cpp
    src/right_gauge_cluster.cpp
    src/cylinder_temperature_gauge.cpp
    src/labeled_gauge.cpp
    src/throttle_display.cpp
    src/afr_cluster.cpp
    src/fuel_cluster.cpp
    src/oscilloscope_cluster.cpp
    src/performance_cluster.cpp
    src/firing_order_display.cpp
    src/load_simulation_cluster.cpp
    src/mixer_cluster.cpp
    src/info_cluster.cpp
    src/rpm_sync.cpp
    src/file_dialog.cpp
    src/accessibility.cpp

    # Include files
    include/delta.h
    include/dtv.h
    include/engine_sim_application.h
    include/geometry_generator.h
    include/simulation_object.h
    include/piston_object.h
    include/connecting_rod_object.h
    include/ui_element.h
    include/ui_manager.h
    include/cylinder_pressure_gauge.h
    include/ui_math.h
    include/units.h
    include/crankshaft_object.h
    include/cylinder_bank_object.h
    include/cylinder_head_object.h
    include/ui_button.h
    include/ui_utilities.h
    include/combustion_chamber_object.h
    include/oscilloscope.h
    include/shaders.h
    include/engine_view.h
    include/right_gauge_cluster.h
    include/cylinder_temperature_gauge.h
    include/labeled_gauge.h
    include/throttle_display.h
    include/afr_cluster.h
    include/fuel_cluster.h
    include/oscilloscope_cluster.h
    include/performance_cluster.h
    include/firing_order_display.h
    include/load_simulation_cluster.h
    include/mixer_cluster.h
    include/info_cluster.h
    include/rpm_sync.h
    include/file_dialog.h
    include/accessibility.h
)

if (DTV)
    target_link_libraries(engine-sim-app
        direct-to-video)
endif (DTV)

if (PIRANHA_ENABLED)
    target_link_libraries(engine-sim-app
        engine-sim-script-interpreter)
endif (PIRANHA_ENABLED)

target_include_directories(engine-sim-app
    PUBLIC dependencies/submodules)

add_subdirectory(dependencies)

if (DISCORD_ENABLED)
    target_sources(engine-sim-app PUBLIC
        src/discord.cpp
        include/discord.h
    )
    target_include_directories(engine-sim-app PUBLIC
        dependencies/discord/include
    )
    target_link_libraries(engine-sim-app
        discord-rpc
    )
endif (DISCORD_ENABLED)

target_link_libraries(engine-sim-app
    engine-sim
)

# ========================================================
# SCREEN READER OUTPUT
#
# Speech and braille through the user's screen reader (docs/accessibility.html)
# use Prism, found through its CMake package (e.g. from vcpkg, via
# CMAKE_PREFIX_PATH). Without it the application builds with speech disabled.

add_library(engine-sim-speech STATIC
    src/screen_reader.cpp
    include/screen_reader.h
)

find_package(prism CONFIG QUIET)
if (TARGET prism::prism)
    target_compile_definitions(engine-sim-speech PRIVATE ENGINE_SIM_SPEECH)
    target_link_libraries(engine-sim-speech PRIVATE prism::prism)
    message(STATUS "Screen reader output: enabled (Prism)")
else ()
    message(STATUS "Screen reader output: Prism not found, disabled")
endif ()

target_link_libraries(engine-sim-app
    engine-sim-speech
)

# Winsock for the UDP RPM sync receiver, comdlg32 for the file dialogs
if (WIN32)
    target_link_libraries(engine-sim-app
        ws2_32
        comdlg32
    )
endif (WIN32)

# ========================================================
# Generated build info

# Fallback to '..' if not provided
if (NOT DEFINED ENGINE_SIM_DATA_ROOT)
    set(ENGINE_SIM_DATA_ROOT ".." CACHE STRING "Where to look for data")
endif()
message(STATUS "Data root set to '${ENGINE_SIM_DATA_ROOT}'")

configure_file(include/build_info.h.in
    generated/build_info.h @ONLY)
target_include_directories(engine-sim-app
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

# ========================================================
# GTEST

enable_testing()

add_executable(engine-sim-test
    # Source files
    test/gas_system_tests.cpp
    test/function_test.cpp
    test/synthesizer_tests.cpp
    test/spoken_text_tests.cpp
    test/engine_list_tests.cpp
)

target_link_libraries(engine-sim-test
    gtest_main
    engine-sim
)

# The engine list tests read the bundled engine scripts
target_compile_definitions(engine-sim-test
    PRIVATE ENGINE_SIM_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

include(GoogleTest)
# Discover at test time: listing the tests runs the executable, which fails
# during the build if runtime DLLs (SDL2, ...) aren't next to it yet
gtest_discover_tests(engine-sim-test DISCOVERY_MODE PRE_TEST)

# Headless forced-induction probe (no window, no audio device)
if (PIRANHA_ENABLED)
    add_executable(boost-probe tools/boost_probe.cpp)
    target_link_libraries(boost-probe engine-sim engine-sim-script-interpreter)
    target_include_directories(boost-probe PUBLIC dependencies/submodules)

    # Headless gear shift probe
    add_executable(shift-probe tools/shift_probe.cpp)
    target_link_libraries(shift-probe engine-sim engine-sim-script-interpreter)
    target_include_directories(shift-probe PUBLIC dependencies/submodules)
endif (PIRANHA_ENABLED)

# ========================================================
# GAME AUDIO EXPORT
#
# engine-sim-export renders seamless RPM loops, transients and sweeps for use
# in games (see docs/game-audio-export.md). WAV is always written; Ogg Vorbis
# and MP3 need libvorbis and LAME, found through their CMake packages (e.g.
# from vcpkg, via CMAKE_PREFIX_PATH or a vcpkg toolchain).

cmake_dependent_option(GAME_AUDIO_EXPORT "Build the game audio exporter (engine-sim-export)" ON
    "PIRANHA_ENABLED" OFF)

if (GAME_AUDIO_EXPORT)
    add_library(engine-sim-game-audio STATIC
        # Source files
        src/game_audio/audio_encoders.cpp
        src/game_audio/engine_session.cpp
        src/game_audio/exporter.cpp
        src/game_audio/json_writer.cpp
        src/game_audio/loop_builder.cpp
        src/game_audio/loudness.cpp
        src/game_audio/wav_file.cpp

        # Include files
        include/game_audio/audio_encoders.h
        include/game_audio/engine_session.h
        include/game_audio/exporter.h
        include/game_audio/json_writer.h
        include/game_audio/loop_builder.h
        include/game_audio/loudness.h
        include/game_audio/wav_file.h
    )

    target_link_libraries(engine-sim-game-audio
        PUBLIC
            engine-sim
            engine-sim-script-interpreter)

    target_include_directories(engine-sim-game-audio
        PUBLIC dependencies/submodules)

    find_package(Ogg CONFIG QUIET)
    find_package(Vorbis CONFIG QUIET)
    if (TARGET Vorbis::vorbisenc AND TARGET Ogg::ogg)
        target_compile_definitions(engine-sim-game-audio PUBLIC ENGINE_SIM_EXPORT_VORBIS)
        target_link_libraries(engine-sim-game-audio PUBLIC Vorbis::vorbisenc Vorbis::vorbis Ogg::ogg)
        message(STATUS "engine-sim-export: Ogg Vorbis enabled")
    else ()
        message(STATUS "engine-sim-export: libvorbis not found, Ogg Vorbis disabled")
    endif ()

    find_package(mp3lame CONFIG QUIET)
    if (TARGET mp3lame::mp3lame)
        target_compile_definitions(engine-sim-game-audio PUBLIC ENGINE_SIM_EXPORT_MP3)
        target_link_libraries(engine-sim-game-audio PRIVATE mp3lame::mp3lame)
        message(STATUS "engine-sim-export: MP3 enabled")
    else ()
        message(STATUS "engine-sim-export: LAME not found, MP3 disabled")
    endif ()

    add_executable(engine-sim-export
        src/game_audio/export_main.cpp)

    target_link_libraries(engine-sim-export
        engine-sim-game-audio)

    add_executable(engine-sim-game-audio-test
        test/game_audio_tests.cpp)

    target_link_libraries(engine-sim-game-audio-test
        gtest_main
        engine-sim-game-audio)

    if (TARGET Vorbis::vorbisfile)
        target_link_libraries(engine-sim-game-audio-test Vorbis::vorbisfile)
    endif ()

    # The integration tests load the bundled engine scripts.
    target_compile_definitions(engine-sim-game-audio-test
        PRIVATE ENGINE_SIM_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

    gtest_discover_tests(engine-sim-game-audio-test DISCOVERY_MODE PRE_TEST)
endif (GAME_AUDIO_EXPORT)

# ========================================================
# ENGINE BUILDER
#
# tools/engine-builder writes engine scripts from a form. If Node is
# installed, test its generator; the slow test (skip with ctest -LE slow) also
# runs every preset on the dyno and through engine-sim-export.

find_program(NODE_EXECUTABLE node)
if (NODE_EXECUTABLE)
    add_test(NAME engine-builder-generator
        COMMAND ${NODE_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/engine-builder/test/generate-presets.mjs
            --out ${CMAKE_CURRENT_BINARY_DIR}/engine-builder-test)

    if (PIRANHA_ENABLED AND TARGET engine-sim-export)
        add_test(NAME engine-builder-engines
            COMMAND ${NODE_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/engine-builder/test/generate-presets.mjs
                --out ${CMAKE_CURRENT_BINARY_DIR}/engine-builder-engines
                --probe $<TARGET_FILE:boost-probe>
                --export $<TARGET_FILE:engine-sim-export>
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
        set_tests_properties(engine-builder-engines PROPERTIES LABELS slow TIMEOUT 1200)
    endif ()
endif ()
