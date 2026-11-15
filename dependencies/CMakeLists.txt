add_subdirectory(submodules)

if (DISCORD_ENABLED)
    # discord-rpc reformats its own sources during the build when it finds
    # clang-format, and current clang-format rejects its .clang-format
    set(CLANG_FORMAT_CMD OFF CACHE FILEPATH "clang-format for discord-rpc (off)")
    add_subdirectory(discord)

    # Its bundled rapidjson 1.1.0 assigns to a const member in a template that
    # is never used; GCC 14 and later reject that unless told not to
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14)
        target_compile_options(discord-rpc PRIVATE -Wno-template-body)
    endif ()
endif ()
