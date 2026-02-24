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
sudo apt install heaptrack heaptrack-gui
heaptrack ./build/vxen
sudo perf record -F 9999 -g --call-graph=dwarf ./build/vxen
sudo perf record -F 9999 -g -e branches,branch-misses ./build/experiment
sudo perf stat -e branches,branch-misses ./build/experiment
sudo perf report
./build/experiment --benchmark_repetitions=5 --benchmark_report_aggregates_only=true

# To set all cpus to performance mode to help prevent cpu impact on all performance stuff & benchmarking
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
sudo cpupower frequency-set -g performance
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Benchmark and check
# Modify the same test block and rerun
./build/experiment --benchmark_out=baseline.json --benchmark_out_format=json --benchmark_repetitions=20
./build/experiment --benchmark_out=contender.json --benchmark_out_format=json --benchmark_repetitions=20
python3 ~/benchmark/tools/compare.py benchmarks baseline.json contender.json
```

---

## Greedy mesh

Release mode

```bash
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.0680099 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.0712837 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.0917326 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.0814183 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.100206 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.0780194 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.115702 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.0519547 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.151858 ms (average) over 1000 iterations
```

Debug mode

```bash
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.250019 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.239719 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.347815 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.358445 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.372301 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.396542 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.204637 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.462924 ms (average) over 1000 iterations
LOG /home/joshua/Youtube/VoxelEngine/src/Kitagawa/ChunkManager.cpp:88 (operator()): Took: 0.513749 ms (average) over 1000 iterations
```

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
- [x] Add chunking again
- [ ] Add LOD
- [ ] Add frustum culling of chunks
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
      API: - std::atomic<uint64_t> m_Flags; - std::queue<DirtyChunk> m_DirtyChunks; - Signal::Set(WORLD_FLAG_CHUNK_MANAGER_SYNC_UPDATE_BIT) // different flags for RENDER_BIT, UPDATE_BIT - Signal::Queue(DirtyChunk{0,0,0}) // The queue must be protected with a mutex - Signal::Pop<DirtyChunk>(); - if (m_Flags & WORLD_FLAG_CHUNK_MANAGER_SYNC_UPDATE_BIT) atomically read & clear the flag & start popping DirtyChunks (copy the queue & clear it);
- [ ] Make the world round and vast to make it seemingly infinate, like the real world and keep all coordinates positive
      - Research octahedral projection mapping http://www.raytracerchallenge.com/bonus/texture-mapping.html
- [ ] Add multithreading to chunk manager.
- [ ] Handle a vertex buffer pool & multiple flat SVO's for raytracing. Seperate by chunk.

- Retire & copy only when modified
- Retire & copy only if parent trail is not copied


**************************************************************************************

With Minimum Allocations

Allocations: 108,549
SVO: 8.7 MB
RCU: 2.1 MB

-----------------------------------------------------------------------------------
Benchmark                                         Time             CPU   Iterations
-----------------------------------------------------------------------------------
BM_SVO_Set_With_RCU                             113 ns          113 ns      6070581
BM_SVO_Set_With_Copy                           76.9 ns         76.9 ns     13007392
BM_SVO_Set_With_Copy_With_Atomic_Root          91.5 ns         91.5 ns      7585624
BM_SVO_Set_Without_Copy                        12.7 ns         12.7 ns     55099298
BM_SVO_Set_Without_Copy_With_Atomic_Root       13.9 ns         13.9 ns     50223700

**************************************************************************************

With Maximum Allocation

Allocations: 256,766
SVO: 9.4 MB
RCU: 4.2 MB

-----------------------------------------------------------------------------------
Benchmark                                         Time             CPU   Iterations
-----------------------------------------------------------------------------------
BM_SVO_Set_With_RCU                             582 ns          582 ns      1245112
BM_SVO_Set_With_Copy                           68.9 ns         68.9 ns     12810929
BM_SVO_Set_With_Copy_With_Atomic_Root          91.6 ns         91.6 ns      7599640
BM_SVO_Set_Without_Copy                        12.7 ns         12.7 ns     54930760
BM_SVO_Set_Without_Copy_With_Atomic_Root       13.9 ns         13.9 ns     50268293

**************************************************************************************

With Minimum Allocations by Cloned parent check

Allocations: 69,140
SVO: 5.5 MB
RCU: 0.5 MB

-----------------------------------------------------------------------------------
Benchmark                                         Time             CPU   Iterations
-----------------------------------------------------------------------------------
BM_SVO_Set_With_RCU                             112 ns          112 ns      6257665
BM_SVO_Set_With_RCU_LESS_CLONES                94.1 ns         94.1 ns     10418613
BM_SVO_Set_With_Copy                           64.8 ns         64.8 ns     13172319
BM_SVO_Set_With_Copy_With_Atomic_Root          92.0 ns         91.9 ns      7608596
BM_SVO_Set_Without_Copy                        12.8 ns         12.7 ns     54982739
BM_SVO_Set_Without_Copy_With_Atomic_Root       14.1 ns         14.1 ns     49666065