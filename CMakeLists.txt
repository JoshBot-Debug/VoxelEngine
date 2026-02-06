cmake_minimum_required(VERSION 3.22)
project(Kitagawa)

# === Options ===
option(ENABLE_AVX256 "Enable AVX-256 instructions" OFF)
option(ENABLE_OPEN_MP "Enable multithreaded with OpenMP" OFF)
option(ENABLE_STL_DEBUG "Enable STL debug mode and DEBUG macro." OFF)
option(ENABLE_THREAD_SANITIZER "Enable ThreadSanitizer (disables AddressSanitizer)" OFF)
option(ENABLE_VULKAN_VALIDATION "Enable vulkan validation & debug extension" OFF)
option(ENABLE_VMA_STATS_LOG "Log out vulkan memory allocator stats when the application exits" OFF)

# === Compiler Flags ===
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# === Optional AVX ===
if(ENABLE_AVX256)
  add_compile_options(-mavx2)
  message(STATUS "AVX-256 instructions ENABLED.")
else()
  message(STATUS "AVX-256 is DISABLED.")
endif()

# === Vulkan validation ===
if(ENABLE_VULKAN_VALIDATION)
    add_compile_definitions(ENABLE_VULKAN_VALIDATION)
endif()

# === Log VMA stats ===
if(ENABLE_VMA_STATS_LOG)
    add_compile_definitions(ENABLE_VMA_STATS_LOG)
endif()

# === Add vendor libraries ===
add_subdirectory(vendor/imgui-cmake)

# === Source Files ===
file(GLOB_RECURSE SRC_FILES
  ${CMAKE_SOURCE_DIR}/src/*.cpp
  ${CMAKE_SOURCE_DIR}/Akari/*.cpp
)
add_executable(Kitagawa ${SRC_FILES})

# === Static/Shared Object Files ===
file(GLOB_RECURSE SHARED_LIB ${CMAKE_SOURCE_DIR}/bin/*.o)
file(GLOB_RECURSE STATIC_LIB ${CMAKE_SOURCE_DIR}/bin/*.a)
target_sources(Kitagawa PRIVATE ${SHARED_LIB})
target_link_libraries(Kitagawa PRIVATE ${STATIC_LIB})

# === OpenMP ===
if(ENABLE_OPEN_MP)
  find_package(OpenMP REQUIRED)
  if(OpenMP_FOUND)
    target_link_libraries(Kitagawa PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(Kitagawa PRIVATE ENABLE_OPEN_MP)
    message(STATUS "OpenMP multithreading is ENABLED.")
  else()
    message(WARNING "OpenMP requested but NOT FOUND.")
  endif()
else()
  message(STATUS "OpenMP is DISABLED.")
endif()

# === Libraries ===
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(TBB REQUIRED)
find_library(NOISE_LIB noise PATHS /usr/local/lib REQUIRED)
find_library(NOISEUTILS_LIB noiseutils PATHS /usr/local/lib REQUIRED)

target_link_libraries(Kitagawa PRIVATE
  vulkan
  glfw
  TBB::tbb
  imgui
  ${NOISE_LIB}
  ${NOISEUTILS_LIB}
)

# === Include Directories ===
target_include_directories(Kitagawa PRIVATE 
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/Akari
  ${CMAKE_SOURCE_DIR}/external
  ${CMAKE_SOURCE_DIR}/vendor/imgui
  ${CMAKE_SOURCE_DIR}/vendor/imgui/backends
  ${CMAKE_SOURCE_DIR}/vendor/VulkanMemoryAllocator/include
)

# === Optional: ccache (speed up rebuilds) ===
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PROGRAM}")
  message(STATUS "ccache is ENABLED.")
endif()

# === Debug Configuration ===
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG -D_GLIBCXX_DEBUG)

  if(ENABLE_STL_DEBUG)
    target_compile_options(Kitagawa PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wconversion
      -Wsign-conversion
      -Wnull-dereference
      -Wdouble-promotion
      -Wformat=2
      -Wunused
    )

    message(STATUS "STL debug mode and DEBUG macro is ENABLED.")

    if(ENABLE_THREAD_SANITIZER)
      set(SANITIZE_FLAGS "-fsanitize=thread")
      message(STATUS "Using ThreadSanitizer (no ASan/UBSan).")
    else()
      set(SANITIZE_FLAGS "-fsanitize=undefined,address -fno-omit-frame-pointer")
      message(STATUS "Using AddressSanitizer + UndefinedBehaviorSanitizer.")
    endif()

    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 ${SANITIZE_FLAGS}")
    set(CMAKE_LINKER_FLAGS_DEBUG "${SANITIZE_FLAGS}")
  else()
    message(STATUS "STL debug mode and DEBUG macro is DISABLED.")
  endif()
endif()


# === Release Configuration ===
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto=auto -funroll-loops -fomit-frame-pointer -g0 -s -ffast-math")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto=auto")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-flto=auto")

# === Release \w Debug Configuration ===
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer -march=native")