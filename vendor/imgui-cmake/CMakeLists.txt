set(IMGUI_SRC
  ../imgui/imgui.cpp
  ../imgui/imgui_demo.cpp
  ../imgui/imgui_draw.cpp
  ../imgui/imgui_tables.cpp
  ../imgui/imgui_widgets.cpp
  ../imgui/backends/imgui_impl_vulkan.cpp
  ../imgui/backends/imgui_impl_glfw.cpp
)

add_library(imgui STATIC ${IMGUI_SRC})

target_include_directories(imgui PUBLIC
  ../imgui
  ../imgui/backends
)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

target_link_libraries(imgui PUBLIC
  vulkan
  glfw
)