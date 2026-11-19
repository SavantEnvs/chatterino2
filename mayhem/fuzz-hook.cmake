# Injected with -DCMAKE_PROJECT_INCLUDE=<this file> (additive: no upstream CMake file is edited).
# It runs after every project() call; on the top-level one it defers mayhem/fuzz-targets.cmake to
# the end of the top-level CMakeLists.txt, when chatterino-lib / chatterino-mocks exist.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR AND NOT MAYHEM_FUZZ_HOOKED)
    set(MAYHEM_FUZZ_HOOKED ON)
    set(MAYHEM_FUZZ_TARGETS "${CMAKE_CURRENT_LIST_DIR}/fuzz-targets.cmake")
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
        CALL include "${MAYHEM_FUZZ_TARGETS}")
endif()
