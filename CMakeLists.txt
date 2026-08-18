cmake_minimum_required(VERSION 3.10)

project(SemantlePlus C)

if(CMAKE_SYSTEM_NAME MATCHES "Retro68")
    add_application(SemantlePlus
        src/main.c
        src/similarity.c
        TYPE APPL
        CREATOR "SmPl"
    )

    # Keep the 68K application small enough for a stock Macintosh Plus and
    # request a single classic-Mac code segment.
    set_target_properties(SemantlePlus PROPERTIES
        COMPILE_OPTIONS "-ffunction-sections"
        LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-single"
    )
else()
    # The portable similarity engine is also built on the host so it can be
    # tested without an emulator or a classic-Mac cross compiler.
    add_executable(SemantlePlusHost
        src/similarity.c
        tests/test_similarity.c
    )
    target_include_directories(SemantlePlusHost PRIVATE src)
    target_compile_options(SemantlePlusHost PRIVATE -Wall -Wextra -Wpedantic)

    enable_testing()
    add_test(NAME similarity COMMAND SemantlePlusHost)
endif()
