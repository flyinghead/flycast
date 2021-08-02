# Find HHC
#
# HHC_EXECUTABLE

find_program(HHC_EXECUTABLE
  NAMES hhc
  DOC "HTML Help Compiler"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HHC DEFAULT_MSG HHC_EXECUTABLE)

mark_as_advanced(HHC_EXECUTABLE)
