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

# Cache misses
perf stat -r 10 -e L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses ./build/experiment
perf stat -C 0-7 -r 10 -e L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses,l2_request.all,l2_request.miss ./build/experiment
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

- [ ] Greedy mesh on the GPU (maybe cancelled if CPU performs fast enough)
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
- [x] Improve sync between flushes. When a thread is dispatched, it may be completed after OnUpdate() and before OnRender()
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
- [x] Add multithreading to chunk manager.
- [x] Create a VkBuffer pool chunk allocator
  - [x] Allocate
  - [x] Reallocate
  - [x] Grow free slots
  - [ ] Defragment
  - [ ] Causes a memory explosion when called multiple times. Likely because I'm adding commands to the command buffer.
    It was probably not meant to be called 32x32x32 per frame. (definitely a problem. Always create as little commands
  as possible)
- [ ] Flat SVO descriptor with a lookup table for fast access of different chunks's SVO tree.
- [ ] Improve performance of SVO by using a custom allocator
- [ ] Handle tracking and updating of dirty chunks.
- [ ] Handle updating the indirect draw command buffer.
      After generating meshes & LODs on the CPU side
      Create a vkBuffer that contains chunk information that I can use to perform chunk related things in a compute shader
      When a change happens, update the struct at that index in the vkBuffer.
      Launch a compute shader with the number of chunks.
      Pass the camera info, and any other required info.
      In the shader, test the chunk to see if it should be drawn.
      Create the indirect draw command from the compute shader.
      Finally... Draw the indirect buffer!


- Retire & copy only when modified
- Retire & copy only if parent trail is not copied

1. Chunk should now use threads to greedy mesh
2. Chunk manager does not need a GreedyMesh function, rather it needs a "Prepare function". Prepare should mesh all dirty chunks
3. Once all chunks are prepared we need to set the CHUNK_MANAGER_FLUSH_RENDER
4. CreateDescriptorSets() is called in Scene when the screen resizes or a buffer resizes, I need to also call it once Chunks are prepared, need to only set the chunks that are avaliable / have data.
5. Need to use draw indirect to draw chunks that have data. Then use gl_DrawID in the shader to access the correct SVOBuffer (Will be writing many buffers here, one for each chunk that has data. MUST MATCH THE draw indirect vertex buffer set. I.e, the vertex buffer and svo buffer need to be from the same chunk so gl_DrawID can be used to index into the SVO storage buffer)
```glsl
layout(set = 0, binding = 0) readonly buffer SVOBuffers {
    uint data[];
} svoBuffers[MAX_CHUNKS];
```

NOTE: Prepare() is just a quick idea. Ideally, we don't want to check a dirty flag for all chunks to find out which ones to prepare,
There will mostly only ever be 1(edited voxel's chunk) + lod chunk changes(in the future)

NOTE: May want to look into Quiescent State-Based Reclamation

When TSignal::Consume(0, CHUNK_MANAGER_FLUSH_UPDATE) is called, start processing dirty chunks
m_World->Flush()
Once flush is called, set an std::atomic<uint64_t> to the maximum number of threads we will be using.
Once each thread is finished, aquire the atomic and check if it's 1,
If it's 1, TSignal::Set(0, CHUNK_MANAGER_FLUSH_RENDER)
In Scene.h, if the signal is on, update the descriptor sets for all dirty chunks & for each chunk, add an indirect draw command.
Use the svoBuffers mentioned in point 5 above.

Dealing with the padding for greedy meshing is to hard.
I need to query out the bit from the neighbouring engine.
Solution:
Each chunk keeps track of padding.
When chunk manager sets voxels, padding should be calculated.

TODO:
Need to create an allocator. Use one VkBuffer for verticies. Use indirect draw for drawing different offsets in that buffer.
/// TODO: There is a glitch every time we need to resize the buffer and copy content. only when the buffer size is small
The buffer default size in Buffer.h is 1 kb, need to increase it something more reasonably or set it whereever I create a buffer.

1. Create a Signal
    - CHUNK_MANAGER_FLUSH_UPDATE
        - Signals when a chunk has changed.
        - Chunk Requirements:
            - 1 u8   | dirty flag    | based on Set(), Clear() & Flatten()
            - 1 u8   | full or empty | based on if verticies were generated for underground voxels, they may be 1 block that is hidden on all sides & tracked by Set() & Clear()
            - 6 u64  | LOD selected  | based on coord 0,0,0 is where the player is
            - 5 bytes padding left unused
        - CHUNK_MANAGER_FLUSH_VERTICES
            - Signals when verticies are ready to be rendered. | the indirect draw command would have changed.
            - Greedy mesh.
                - Check if full or empty
                - Check distance from 0,0,0 to select LOD
                - Generate LOD if it was not generated already. | the chunk keeps track of this (6 bits somewhere else for lods already existing)
                - Check if voxels exist or not                  | may generate an empty vector if chunk is underground
                - Update the chunk's lookup table data          | LOD selected, full or empty, dirty flag
            - Upload verticies to the GPU                       | Synchronize with the preprocessor indirect buffer
            - Set the signal
        - CHUNK_MANAGER_FLUSH_PREPROCESSOR
            - Signals when FlatNodes & other requirements are read for the preprocessor compute shader.
            - Generate FlatNodes for Chunks
            - Generate LUT
            - Upload to GPU for processing
            - Try to cull underground voxels from flat nodes | This will enable both CPU compute and GPU compute to run in parallel. Save nano seconds!
            - Proprocessor
                - Culls chunks hidden from the camera view  | Frustum culling
                - Builds indirect draw buffer               | Uses the LUT & FlatNode Id to figure out LOD offsets
            - Update? indirect draw buffer
            - Set the signal!!

If it's possible to process both CPU and GPU work in parallel use this to synchronize work:

1. Use a memory barrier to synchronize the compute shader with the draw command
2. If the CPU work is not done, don't submit the draw command
3. Once the CPU work is done, submit the draw command, it'll wait on the memory barrier and eventually the draw command will execute with both data

There seems to be a memory issue, It's definitely in the Buffer::Upload(). Most likely because to copy memory, etc we use
a command buffer. We may update/create a new buffer before the command is executed. There are issues with using
Buffer::Upload() in a loop like how I use it.
The problem seems to be reallocating more memory, causes an explosion.
If I set the buffer size from 1024 to 64mb, it reallocates only once and no explosion happens.
This Buffer:Upload() problem was resolved by calling Upload only once. Initially I was calling Upload() in a for loop.
This is definitely not how I want to do it. So Does buffer.Upload() have a problem?

There are two problems
1. SVO RCU is causing memory to go up but not come down after Sync()
    - Temporarly fixed by removing copies when changes are made. Need to add this back in the future
    - May want to remove RCU entirely. 
2. Buffer resize causes bugs if the Size is too small and we call Upload in quick successions

TODO:
1. Multiple Flattened SVO to the GPU
    - Chunk Manager
        ChunkSO
            - Chunk
                VoxelSO

```glsl
struct Header {
    uint id; // The id is a 1D index into a 3D grid (x + (y * 32 + (z * 32 * 32)))
    uint count; // Number of nodes
    uint offset; // The offset index from where this chunk's svo starts.
};

struct FlatNode {
    uint PackedIDC;
    uint ChildIndex;
}

layout(std430,set=1,binding=50)readonly buffer SparseOctreeLUT{
  Header headers[];
};

layout(std430,set=1,binding=50)readonly buffer SparseOctree{
  FlatNode nodes[];
};
```
