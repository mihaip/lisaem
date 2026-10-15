cmake_minimum_required(VERSION 3.20)
project(LisaEmInfiniteMac C)
set(CMAKE_C_STANDARD 99)
get_filename_component(ROOT "../.." ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(GENERATOR "${ROOT}/lib/libGenerator")
set(GEN "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GEN}/cpu68k" "${GEN}/include")
configure_file(machine.h "${GEN}/include/machine.h" COPYONLY)
file(WRITE "${GEN}/include/built_by.h" "#define VERSION \"Infinite Mac\"\n")

# The generators run on the build host, including when the target is Wasm.
find_program(HOST_CC NAMES cc clang gcc NO_CMAKE_FIND_ROOT_PATH REQUIRED)
set(HOST_INCLUDES "-I${GEN}/include" "-I${GENERATOR}/include")
set(GENERATED_HEADERS)
foreach(name iibs funcs proto)
    list(APPEND GENERATED_HEADERS "${GEN}/include/def68k-${name}.h")
endforeach()
set(OPCODES)
foreach(n 0 1 2 3 4 5 6 7 8 9 a b c d e f)
    list(APPEND OPCODES "${GEN}/cpu68k/cpu68k-${n}.c")
endforeach()
add_custom_command(OUTPUT ${GENERATED_HEADERS} ${OPCODES}
    COMMAND ${CMAKE_COMMAND} -E copy "${GENERATOR}/cpu68k/def68k.def" .
    COMMAND ${HOST_CC} -std=c99 -O2 ${HOST_INCLUDES}
        "${GENERATOR}/cpu68k/def68k.c" "${GENERATOR}/cpu68k/tab68k.c" -o def68k
    COMMAND ./def68k
    # Copy gen68k so its quoted include resolves to the freshly generated table.
    COMMAND ${CMAKE_COMMAND} -E copy "${GENERATOR}/cpu68k/gen68k.c" .
    COMMAND ${HOST_CC} -std=c99 -O2 ${HOST_INCLUDES}
        gen68k.c "${GENERATOR}/cpu68k/tab68k.c" -o gen68k
    COMMAND ./gen68k
    DEPENDS "${GENERATOR}/cpu68k/def68k.c" "${GENERATOR}/cpu68k/def68k.def"
        "${GENERATOR}/cpu68k/gen68k.c" "${GENERATOR}/cpu68k/tab68k.c"
        "${GENERATOR}/include/generator.h" machine.h
    WORKING_DIRECTORY "${GEN}/cpu68k" VERBATIM)
add_custom_target(generate_opcodes DEPENDS ${OPCODES} ${GENERATED_HEADERS})

set(CORE_SOURCES
    storage/hle.c storage/profile.c
    lisa/motherboard/fliflo_queue.c lisa/motherboard/glue.c
    lisa/motherboard/symbols.c lisa/motherboard/unvars.c lisa/motherboard/vars.c
    lisa/cpu_board/irq.c lisa/cpu_board/memory.c lisa/cpu_board/mmu.c
    lisa/cpu_board/rom.c lisa/cpu_board/romless.c
    lisa/io_board/cops.c lisa/io_board/floppy.c lisa/io_board/via6522.c
    lisa/io_board/z8530.c lisa/io_board/etherbox.c
    lisa/io_board/etherbox-responder.c lisa/io_board/etherbox-slirp.c
    lib/libGenerator/generator/cpu68k.c lib/libGenerator/generator/reg68k.c
    lib/libGenerator/generator/diss68k.c lib/libGenerator/generator/ui_log.c
    lib/libGenerator/cpu68k/tab68k.c lib/libdc42/src/libdc42.c
    lib/libdc42/src/lib_raw_profile_image.c
    lib/musashi/m68kcpu.c lib/musashi/m68kops.c lib/musashi/m68kdasm.c
    lib/musashi/softfloat/softfloat.c lib/musashi/cpucheck.c)
list(TRANSFORM CORE_SOURCES PREPEND "${ROOT}/")
add_library(lisa_core STATIC ${CORE_SOURCES} ${OPCODES})
add_dependencies(lisa_core generate_opcodes)
target_include_directories(lisa_core PUBLIC
    "${GEN}/include" "${ROOT}/include" "${GENERATOR}/include"
    "${ROOT}/lib/libdc42/include" "${ROOT}/lib/libdc42/src/include"
    "${ROOT}/lisa/keyboard/include")
target_compile_options(lisa_core PRIVATE -fwrapv -fno-strict-aliasing)
if(EMSCRIPTEN)
    target_compile_definitions(lisa_core PRIVATE INFINITE_MAC)
    target_compile_options(lisa_core PRIVATE -gsource-map)
    add_executable(lisaem main_worker.c emulator.c host_stubs.c audio.c disk.c)
    target_link_libraries(lisaem PRIVATE lisa_core)
    target_compile_options(lisaem PRIVATE -gsource-map)
    target_link_options(lisaem PRIVATE -gsource-map -g2
        "-sMODULARIZE" "-sEXPORT_ES6" "-sEXPORT_NAME=emulator"
        "-sEXPORTED_RUNTIME_METHODS=['FS','HEAPU8']"
        "-sEXPORTED_FUNCTIONS=['_main','_malloc']"
        "-sINITIAL_MEMORY=67108864" "-sALLOW_MEMORY_GROWTH"
        "-sEXIT_RUNTIME=1" "-sENVIRONMENT=worker" -lexports.js)
else()
    add_executable(lisaem-im main_native.c emulator.c host_stubs.c audio.c)
    target_link_libraries(lisaem-im PRIVATE lisa_core)
endif()
