# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.9 FATAL_ERROR)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Schulprojekt")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (false)
# the name of the target operating system
set(CMAKE_SYSTEM_NAME Windows)

# which compilers to use for C++
set(CMAKE_CXX_COMPILER "/usr/bin/x86_64-w64-mingw32-g++")

# where is the target environment located
set(CMAKE_FIND_ROOT_PATH  /usr/i686-w64-mingw32)

# adjust the default behavior of the FIND_XXX() commands:
# search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# search headers and libraries in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

endif()

set (OSBitness 32)
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set (OSBitness 64)
endif()
set (FullOutputDir "${CMAKE_SOURCE_DIR}/out/${CMAKE_SYSTEM_NAME}${OSBitness}/${CMAKE_BUILD_TYPE}")
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${FullOutputDir}/static libs")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${FullOutputDir}")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${FullOutputDir}")

include(FetchContent)

find_package(Vulkan REQUIRED)

set(glm_DIR "/usr/lib32/cmake/glm")
find_package(glm REQUIRED)

FetchContent_Declare (
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL  "")
set(GLFW_INSTALL OFF CACHE BOOL  "")

FetchContent_MakeAvailable(glfw)

# Include sub-projects.
add_subdirectory ("game")
