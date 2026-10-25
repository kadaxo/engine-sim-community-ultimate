# Fixes for pinned submodule commits that aren't published in any fork yet.
# They are applied to the submodule working trees at configure time, see
# patches/README.md.
find_package(Git QUIET)

function(engine_sim_apply_patches target_dir patch_dir)
    file(GLOB patches "${patch_dir}/*.patch")
    list(SORT patches)

    # Patches go on in order, so the newest one that can be reversed marks
    # how far an earlier configure got. Checking each patch on its own fails
    # once a later patch has changed the same lines.
    list(LENGTH patches patch_count)
    set(first_missing 0)
    set(index ${patch_count})
    while (index GREATER 0)
        math(EXPR index "${index} - 1")
        list(GET patches ${index} patch)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${patch}"
            WORKING_DIRECTORY "${target_dir}"
            RESULT_VARIABLE reverse_result
            OUTPUT_QUIET ERROR_QUIET)
        if (reverse_result EQUAL 0)
            math(EXPR first_missing "${index} + 1")
            break()
        endif()
    endwhile()

    set(index 0)
    foreach(patch ${patches})
        get_filename_component(patch_name "${patch}" NAME)
        math(EXPR position "${index}")
        math(EXPR index "${index} + 1")
        if (position LESS first_missing)
            continue()
        endif()

        execute_process(
            COMMAND "${GIT_EXECUTABLE}" apply "${patch}"
            WORKING_DIRECTORY "${target_dir}"
            RESULT_VARIABLE apply_result)
        if (NOT apply_result EQUAL 0)
            message(FATAL_ERROR "Could not apply ${patch_name} to ${target_dir}")
        endif()
        message(STATUS "Applied ${patch_name}")
    endforeach()
endfunction()

if (GIT_FOUND)
    engine_sim_apply_patches(
        "${CMAKE_CURRENT_SOURCE_DIR}/delta-studio"
        "${PROJECT_SOURCE_DIR}/patches/delta-studio")
else()
    message(WARNING "Git not found, can't apply patches/delta-studio (the Windows build will fail)")
endif()

add_subdirectory(delta-studio)

set_property(TARGET delta-basic PROPERTY FOLDER "delta")
set_property(TARGET delta-basic-demo PROPERTY FOLDER "delta")
set_property(TARGET delta-core PROPERTY FOLDER "delta")
set_property(TARGET delta-physics PROPERTY FOLDER "delta")

add_subdirectory(simple-2d-constraint-solver)

set_property(TARGET simple-2d-constraint-solver PROPERTY FOLDER "scs")
set_property(TARGET simple-2d-constraint-solver-test PROPERTY FOLDER "scs")

add_subdirectory(csv-io)

set_property(TARGET csv-io PROPERTY FOLDER "csv-io")
set_property(TARGET csv-io-main PROPERTY FOLDER "csv-io")
set_property(TARGET csv-io-test PROPERTY FOLDER "csv-io")

if (DTV)
    add_subdirectory(direct-to-video)

    set_property(TARGET direct-to-video PROPERTY FOLDER "dtv")
    set_property(TARGET direct-to-video PROPERTY FOLDER "dtv")
endif (DTV)

add_subdirectory(piranha)

set_property(TARGET piranha PROPERTY FOLDER "piranha")
set_property(TARGET piranha_test PROPERTY FOLDER "piranha")
set_property(TARGET piranha_reference_compiler PROPERTY FOLDER "piranha")
