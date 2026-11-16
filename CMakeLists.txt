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

project(engine-sim VERSION 0.2.1)

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
    src/data_root.cpp
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
    src/forced_induction.cpp
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
    include/forced_induction.h
    include/jitter_filter.h
    include/leveling_filter.h
    include/low_pass_filter.h
    include/part.h
    include/piston.h
    include/piston_engine_simulator.h
    include/simulator.h
    include/data_root.h
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
        scripting/include/forced_induction_node.h
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
    test/data_root_tests.cpp
    test/forced_induction_tests.cpp
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

# ========================================================
# RELEASE PACKAGE
#
# `cmake --install build --component release --prefix <dir>` lays out a folder
# that runs on its own: the programs, the engine scripts, the libraries the
# programs load and the licenses. On Windows and Linux everything is in one
# folder; on macOS the programs and data are in "Engine Sim.app", with the
# README and licenses beside it. scripts/package-windows.ps1 and
# scripts/package.sh check the folder and archive it
# (.github/workflows/release.yml). Other install rules (googletest,
# discord-rpc) are in other components.

set(ENGINE_SIM_RELEASE_PROGRAMS engine-sim-app)
if (TARGET engine-sim-export)
    list(APPEND ENGINE_SIM_RELEASE_PROGRAMS engine-sim-export)
endif ()

if (APPLE)
    set(ENGINE_SIM_BUNDLE "Engine Sim.app")
    set(_release_programs "${ENGINE_SIM_BUNDLE}/Contents/MacOS")
    set(_release_data "${ENGINE_SIM_BUNDLE}/Contents/Resources")
else ()
    set(_release_programs .)
    set(_release_data .)
endif ()

# vcpkg (or other) prefixes the build took libraries from, with forward
# slashes: the paths end up in the install script, which would read the
# backslashes of a path such as D:\a\... as escapes
set(_prefixes ${CMAKE_PREFIX_PATH})
if (DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    list(APPEND _prefixes "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
endif ()
set(ENGINE_SIM_LIBRARY_PREFIXES)
foreach (_prefix IN LISTS _prefixes)
    string(REPLACE "\\" "/" _prefix "${_prefix}")
    list(APPEND ENGINE_SIM_LIBRARY_PREFIXES "${_prefix}")
endforeach ()
list(REMOVE_DUPLICATES ENGINE_SIM_LIBRARY_PREFIXES)

if (WIN32 AND NOT CMAKE_VERSION VERSION_LESS 3.21)
    # The DLLs the programs load (SDL2, and the encoders when they are DLLs),
    # looked for beside the import libraries they were linked with
    set(_release_dll_dirs)
    foreach (_lib IN LISTS SDL2_LIBRARY SDL2_IMAGE_LIBRARY)
        if (EXISTS "${_lib}")
            get_filename_component(_dir "${_lib}" DIRECTORY)
            list(APPEND _release_dll_dirs "${_dir}" "${_dir}/../bin")
        endif ()
    endforeach ()
    foreach (_prefix IN LISTS ENGINE_SIM_LIBRARY_PREFIXES)
        list(APPEND _release_dll_dirs "${_prefix}/bin")
    endforeach ()

    # Leave out Windows itself (API sets, system32, d3dcompiler_47 since
    # Windows 10) and the C++ runtime, which comes from Visual Studio below.
    # CMake matches DLL names in lowercase, but folders keep their case.
    # The patterns take either slash, so CMake 4.3's forward-slash paths
    # (policy CMP0207) suit them too.
    if (POLICY CMP0207)
        cmake_policy(SET CMP0207 NEW)
    endif ()
    install(TARGETS ${ENGINE_SIM_RELEASE_PROGRAMS}
        RUNTIME_DEPENDENCIES
            PRE_EXCLUDE_REGEXES
                "^api-ms-" "^ext-ms-" "^d3dcompiler_47\\.dll$"
                "^vcruntime" "^msvcp" "^concrt" "^vccorlib" "^ucrtbase"
            POST_EXCLUDE_REGEXES
                ".*[/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*"
                ".*[/\\\\][Ss][Yy][Ss][Ww][Oo][Ww]64[/\\\\].*"
                ".*[/\\\\][Ww][Ii][Nn][Ss][Xx][Ss][/\\\\].*"
            DIRECTORIES ${_release_dll_dirs}
        RUNTIME DESTINATION . COMPONENT release)

    if (MSVC)
        # The Visual C++ runtime DLLs, so the app runs without the
        # redistributable installer
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT release)
        include(InstallRequiredSystemLibraries)
    endif ()
else ()
    # macOS and Linux downloads link the vcpkg libraries statically; on Linux
    # SDL2 and SDL2_image come from the system
    install(TARGETS ${ENGINE_SIM_RELEASE_PROGRAMS}
        RUNTIME DESTINATION "${_release_programs}" COMPONENT release)
endif ()

if (APPLE)
    if (CMAKE_OSX_DEPLOYMENT_TARGET)
        set(ENGINE_SIM_MACOS_MINIMUM "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else ()
        set(ENGINE_SIM_MACOS_MINIMUM 11.0)
    endif ()
    configure_file(cmake/release/Info.plist.in
        ${CMAKE_CURRENT_BINARY_DIR}/release-files/Info.plist @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/release-files/Info.plist
        DESTINATION "${ENGINE_SIM_BUNDLE}/Contents" COMPONENT release)
endif ()

# Engine scripts and the script library; the app finds them beside itself,
# or in Contents/Resources inside the macOS app (see data_root::find)
install(DIRECTORY assets es
    DESTINATION "${_release_data}" COMPONENT release
    PATTERN "probe_*.mr" EXCLUDE)

# delta-studio's shaders and the console font, in delta/ where the app looks
# for them before the submodule's folder
set(_delta_basic dependencies/submodules/delta-studio/engines/basic)
install(DIRECTORY ${_delta_basic}/shaders/
    DESTINATION "${_release_data}/delta/shaders" COMPONENT release)
install(DIRECTORY ${_delta_basic}/fonts/Silkscreen
    DESTINATION "${_release_data}/delta/fonts" COMPONENT release)

# Documentation, and the engine builder at the path the guide links to
# (the guide opens with F4, from the data folder)
install(FILES README.md COMMUNITY.md LICENSE RPM_SYNC.md
    DESTINATION . COMPONENT release)
install(DIRECTORY docs
    DESTINATION "${_release_data}" COMPONENT release)
install(FILES
        tools/engine-builder/index.html
        tools/engine-builder/builder.js
        tools/engine-builder/generator.js
    DESTINATION "${_release_data}/tools/engine-builder" COMPONENT release)

# Licenses: this project's, the bundled libraries' and those of the packages
# the build took from vcpkg (share/<port>/copyright)
install(FILES cmake/release/licenses-readme.txt
    DESTINATION licenses RENAME README.txt COMPONENT release)
install(FILES LICENSE
    DESTINATION licenses RENAME engine-sim.txt COMPONENT release)
foreach (_dep IN ITEMS delta-studio piranha simple-2d-constraint-solver csv-io)
    install(FILES dependencies/submodules/${_dep}/LICENSE
        DESTINATION licenses RENAME ${_dep}.txt COMPONENT release)
endforeach ()
if (DISCORD_ENABLED)
    install(FILES dependencies/discord/LICENSE
        DESTINATION licenses RENAME discord-rpc.txt COMPONENT release)
endif ()
install(FILES "${_delta_basic}/fonts/Silkscreen/Kottke Silkscreen License.txt"
    DESTINATION licenses RENAME silkscreen-font.txt COMPONENT release)
foreach (_prefix IN LISTS ENGINE_SIM_LIBRARY_PREFIXES)
    file(GLOB _copyrights "${_prefix}/share/*/copyright")
    foreach (_copyright IN LISTS _copyrights)
        get_filename_component(_port_dir "${_copyright}" DIRECTORY)
        get_filename_component(_port "${_port_dir}" NAME)
        if (NOT _port MATCHES "^vcpkg-")
            install(FILES "${_copyright}"
                DESTINATION licenses RENAME ${_port}.txt COMPONENT release)
        endif ()
    endforeach ()
endforeach ()
