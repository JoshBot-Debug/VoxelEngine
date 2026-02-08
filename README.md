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

Benchmark / Performance

```bash
sudo perf record -F 999 -g --call-graph=dwarf ./build/Kitagawa
sudo perf report
```

---

## TODO

- [ ] Greedy mesh on the GPU
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
- [x] MAJOR: Rework the archtecture of the code base in this scene. GOAL: Bring the design slightly closer to something more satisfactory.
- [ ] Add chunking again
- [ ] Add LOD
- [ ] Add trees
- [x] Add a skybox
- [x] Make the SVO a sparse octree, the datatype associated with it should be templated.
- [x] Fix memory leaks - NOTE: Fixed it be removing COW, no segment faults during testing
- [x] Move greedy meshing out of SparseOctree because it depends on material
- [x] Added basic reflections via raymarching
- [x] Make the SVO thread safe. COW works a bit, main problem cleanup, take a page from ECS bitset usage
  - [x] Fixed by implementing RCU. https://www.youtube.com/watch?v=rxQ5K9lo034
- [x] Improve greedy meshing
  - [x] Add .GetAxis() - It should return a mask (uint64_t mask[4096]) of x axis for a material
  - [x] Remove the mask generated in GreedyMesh64, it's per voxel and too slow.
- [x] Threads clone & start are very slow. Need to implement a thread pool.
- [ ] Improve sync between flushes. When a thread is dispatched, it may be completed after OnUpdate() and before OnRender()
      OnRender() clears the dirty flag and OnUpdate never see the flag. Nothing is rendered to the screen.
      Proposal: World::Flag(CHUNK_MANAGER_SYNC_BIT), World::Flag(CHUNK_MANAGER_FLUSH_BIT), World::Flag(PALETTE_FLUSH_BIT)
      At the start of OnUpdate(), call World::Sync(), it should check all FLAGS and Flush() / Sync() / Others...
      This way when a thread completes it's job, if it's after OnUpdate() and before OnRender(), No dirty flags will be toggled
      They will toggle when World::Sync() is called, at the start of OnUpdate(). Hence fixes the sync problem
      Create a class to enable synchronization across the application, remove dirty flags from the SVO, palette implementations
      Create a queue in the class to handle arbatry information needed to process dirty parts efficiently, like chunk id/coord
      API:
        - std::atomic<uint64_t> m_Flags;
        - std::queue<DirtyChunk> m_DirtyChunks;
        - Synchronization::Set(WORLD_FLAG_CHUNK_MANAGER_SYNC_UPDATE_BIT) // different flags for RENDER_BIT, UPDATE_BIT
        - Synchronization::Queue(DirtyChunk{0,0,0}) // The queue must be protected with a mutex
        - Synchronization::Pop<DirtyChunk>();
        - if (m_Flags & WORLD_FLAG_CHUNK_MANAGER_SYNC_UPDATE_BIT) atomically read & clear the flag & start popping DirtyChunks (copy the queue & clear it);
