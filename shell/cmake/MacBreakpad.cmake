# Keep the pinned upstream submodule intact. Its macOS DWARF 5 reader needs
# two fixes; apply them to a build-tree copy used only by the symbol exporter.
set(_breakpad_source "${CMAKE_CURRENT_SOURCE_DIR}/core/deps/breakpad")
set(_breakpad_copy "${CMAKE_CURRENT_BINARY_DIR}/breakpad/source")
set(_breakpad_patch "${CMAKE_CURRENT_SOURCE_DIR}/shell/apple/breakpad-dwarf5.patch")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_breakpad_patch}"
    "${_breakpad_source}/src/common/dwarf/dwarf2reader.cc"
    "${_breakpad_source}/src/common/mac/dump_syms.cc")
file(COPY "${_breakpad_source}/src" DESTINATION "${_breakpad_copy}"
    PATTERN ".git" EXCLUDE PATTERN "build" EXCLUDE)
# Restore these files before patching, including on a repeated configure.
foreach(_file IN ITEMS common/dwarf/dwarf2reader.cc common/mac/dump_syms.cc)
    configure_file("${_breakpad_source}/src/${_file}"
        "${_breakpad_copy}/src/${_file}" COPYONLY)
endforeach()
find_program(FLYCAST_PATCH_EXECUTABLE patch REQUIRED)
execute_process(COMMAND "${FLYCAST_PATCH_EXECUTABLE}" --batch --forward -p1 -i "${_breakpad_patch}"
    WORKING_DIRECTORY "${_breakpad_copy}"
    RESULT_VARIABLE _breakpad_patch_result
    OUTPUT_VARIABLE _breakpad_patch_output ERROR_VARIABLE _breakpad_patch_error)
if(NOT _breakpad_patch_result EQUAL 0)
    message(FATAL_ERROR "Cannot apply the macOS Breakpad DWARF 5 fixes: ${_breakpad_patch_output}${_breakpad_patch_error}")
endif()
set(FLYCAST_DUMP_SYMS "${CMAKE_CURRENT_BINARY_DIR}/breakpad/bin/dump_syms")
add_custom_target(dump_syms
    COMMAND xcodebuild -project "${_breakpad_copy}/src/tools/mac/dump_syms/dump_syms.xcodeproj"
        -target dump_syms -configuration Release
        "CONFIGURATION_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}/breakpad/bin"
        "OBJROOT=${CMAKE_CURRENT_BINARY_DIR}/breakpad/obj"
        "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}/breakpad/products"
        GCC_TREAT_WARNINGS_AS_ERRORS=NO
    VERBATIM)
