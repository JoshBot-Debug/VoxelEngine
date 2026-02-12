cmake_minimum_required(VERSION 3.22)
project(vxen)

# === Options ===
option(ENABLE_AVX256 "Enable AVX-256 instructions" OFF)
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
file(GLOB_RECURSE SRC_FILES_AKARI ${CMAKE_SOURCE_DIR}/akari/*.cpp)
file(GLOB_RECURSE SRC_FILES_VXENGINE ${CMAKE_SOURCE_DIR}/engine/*.cpp)
file(GLOB_RECURSE SRC_FILES_VXEN ${CMAKE_SOURCE_DIR}/app/main.cpp)
file(GLOB_RECURSE SRC_FILES_EXPERIMENT ${CMAKE_SOURCE_DIR}/experiment/main.cpp)

# === Add libraries ===
add_library(akari ${SRC_FILES_AKARI})
add_library(vxengine ${SRC_FILES_VXENGINE})
add_executable(vxen ${SRC_FILES_VXEN})
add_executable(experiment ${SRC_FILES_EXPERIMENT})

# === Link Libraries ===
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(TBB REQUIRED)
find_library(NOISE_LIB noise PATHS /usr/local/lib REQUIRED)
find_library(NOISEUTILS_LIB noiseutils PATHS /usr/local/lib REQUIRED)

target_link_libraries(akari PUBLIC
  vulkan
  glfw
)

target_link_libraries(vxengine PUBLIC
  akari
  TBB::tbb
  imgui
  ${NOISE_LIB}
  ${NOISEUTILS_LIB}
)

target_link_libraries(vxen PRIVATE vxengine)
target_link_libraries(experiment PRIVATE vxengine)

# === Include Directories ===
target_include_directories(akari PUBLIC 
  ${CMAKE_SOURCE_DIR}/akari
  ${CMAKE_SOURCE_DIR}/vendor/imgui
  ${CMAKE_SOURCE_DIR}/vendor/imgui/backends
  ${CMAKE_SOURCE_DIR}/vendor/VulkanMemoryAllocator/include
)

target_include_directories(vxengine PUBLIC 
  ${CMAKE_SOURCE_DIR}/engine
  ${CMAKE_SOURCE_DIR}/external
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
    target_compile_options(akari vxengine PRIVATE
      $<$<CONFIG:Debug>:
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
      >
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


# === Release \w Debug Configuration ===
target_compile_options(akari PRIVATE
  $<$<CONFIG:RelWithDebInfo>:-O3 -g -fno-inline -fno-omit-frame-pointer -march=native>
)

target_compile_options(vxengine PRIVATE
  $<$<CONFIG:RelWithDebInfo>:-O3 -g -fno-inline -fno-omit-frame-pointer -march=native>
)


# === Release Configuration ===
target_compile_options(akari PRIVATE
  $<$<CONFIG:Release>:-O3 -march=native -flto=auto -funroll-loops -fomit-frame-pointer -ffast-math>
)

target_compile_options(vxengine PRIVATE
  $<$<CONFIG:Release>:-O3 -march=native -flto=auto -funroll-loops -fomit-frame-pointer -ffast-math>
)

target_link_options(akari PRIVATE
  $<$<CONFIG:Release>:-flto=auto>
)

target_link_options(vxengine PRIVATE
  $<$<CONFIG:Release>:-flto=auto>
)