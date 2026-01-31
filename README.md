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
- [x] Add the ability to create/update/delete voxels (sandbox tools) - to help improve debugging, etc
- [x] MAJOR: Rework how render pass & pipelines are created
- [ ] Add a version 1 toolbox. GOAL: Create something pretty
  - [ ] Add a transparent floating voxel where the cursor is placed.
  - [ ] Add a way to change the size of the voxel block being created (increase/decrease size by scrolling)
  - [ ] Add a way to drag to increate number of voxels that can be created in one shot
  - [ ] Add a way to create prefabs
  - [ ] Add a way to scale(x,y,z), rotate(x,y,z), translate(x,y,z) a prefab
- [ ] MAJOR: Rework the archtecture of the code base in this scene. GOAL: Bring the design slightly closer to something more satisfactory.
- [ ] Add chunking again
- [ ] Add LOD
- [x] Add a skybox
- [x] Make the SVO a sparse octree, the datatype associated with it should be templated.
- [ ] Add a garbage collector for SVO deleted nodes