# Vk Voxel Playground

### Setup

Initialize all submodules

```bash
git submodule add https://github.com/ocornut/imgui.git vendor/imgui
git submodule add https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git vendor/VulkanMemoryAllocator
git submodule update --init --recursive
cd vendor/imgui
git checkout docking
cd ../..
```

Compile shaders

```bash
bash compileShaders.sh
```

Build project

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
```

Run the project

```bash
./build/Kitagawa
```

---

## TODO
- [x] Greedy mesh on the GPU
- [ ] All voxels that are disjointed need to be in a seperate SVO.
- [x] Implement copy on write (cow) for the SVO
- [ ] Add the ability to render rays (raymarching for debug and stuff)
- [ ] Add the ability to create/update/delete voxels (sandbox tools) - to help improve debugging, etc