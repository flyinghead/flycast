cmake_minimum_required(VERSION 3.12)

if(NOT DEFINED SOURCE_DIR)
	set(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
endif()

set(CHECK_NO_STD_FORMAT_EXTENSIONS
	*.h
	*.hh
	*.hpp
	*.hxx
	*.c
	*.cc
	*.cpp
	*.cxx
	*.mm
)

set(CHECK_NO_STD_FORMAT_PATTERN
	"^[ \t]*#[ \t]*include[ \t]*<[ \t]*format[ \t]*>"
)

set(violations)
set(files)

find_program(GIT_EXECUTABLE git)
if(NOT GIT_EXECUTABLE)
	message(STATUS "Skipping CheckNoStdFormat: git was not found.")
	return()
endif()

if(NOT EXISTS "${SOURCE_DIR}/.git")
	message(STATUS "Skipping CheckNoStdFormat: ${SOURCE_DIR} is not a git worktree.")
	return()
endif()

execute_process(
	COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" diff --name-only --diff-filter=ACMR --
	RESULT_VARIABLE git_worktree_result
	OUTPUT_VARIABLE git_worktree_output
	ERROR_QUIET
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
	COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" diff --cached --name-only --diff-filter=ACMR --
	RESULT_VARIABLE git_cached_result
	OUTPUT_VARIABLE git_cached_output
	ERROR_QUIET
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
	COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" ls-files --others --exclude-standard --
	RESULT_VARIABLE git_untracked_result
	OUTPUT_VARIABLE git_untracked_output
	ERROR_QUIET
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(git_worktree_result EQUAL 0 AND git_cached_result EQUAL 0 AND git_untracked_result EQUAL 0)
	set(git_changed_output "${git_worktree_output}\n${git_cached_output}\n${git_untracked_output}")
	string(REPLACE "\n" ";" files "${git_changed_output}")
	list(REMOVE_ITEM files "")
	list(REMOVE_DUPLICATES files)
endif()

foreach(relpath IN LISTS files)
	set(path "${SOURCE_DIR}/${relpath}")
	if(NOT relpath MATCHES "^core/.*\\.(h|hh|hpp|hxx|c|cc|cpp|cxx|mm)$")
		continue()
	endif()
	if(path MATCHES "/core/deps/" OR path MATCHES "\\.orig$")
		continue()
	endif()

	file(STRINGS "${path}" lines)
	set(line_number 0)
	foreach(line IN LISTS lines)
		math(EXPR line_number "${line_number} + 1")
		if(line MATCHES "${CHECK_NO_STD_FORMAT_PATTERN}")
			list(APPEND violations
				"${path}:${line_number}: Do not include <format> in Flycast code. Use #include <fmt/format.h> and fmt::format(...) instead.")
		endif()
	endforeach()
endforeach()

list(LENGTH violations violation_count)
if(violation_count GREATER 0)
	message(STATUS "Found forbidden <format> includes:")
	foreach(violation IN LISTS violations)
		message(STATUS "  ${violation}")
	endforeach()
	message(FATAL_ERROR "<format> is forbidden in Flycast code. Use #include <fmt/format.h> and fmt::format(...) instead.")
endif()
