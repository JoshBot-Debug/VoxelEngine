#include <benchmark/benchmark.h>
#include <vector>

#include "window/Application.h"
bool g_ApplicationRunning {false};

#include "render/Buffer.h"
#include "voxel/Palette.h"
#include "voxel/SparseOctree.h"
#include "voxel/SparseOctreeWithoutRCU.h"
#include "voxel/Voxel.h"

#include <random>

const int ITERATIONS = 1;

static void BM_SVO_Set_With_RCU_With_Copy_Check(benchmark::State& state) {
  SparseOctree<Voxel> svo;
  Palette             palette;

  palette.Create(Palette::Item {
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material {
           .Albedo = glm::vec4 {0.63f, 0.067f, 0.051f, 1.0f}})});

  auto     brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);
  uint64_t i     = 0;

  for (auto _ : state) {
    {
      auto     w = svo.BeginWrite();
      uint64_t x = i & 63;
      uint64_t y = (i >> 6) & 63;
      uint64_t z = (i >> 12) & 63;
      svo.Set(w, x, y, z, brick.get());
      i = (i + 1) & 262143;
    }
    svo.Sync();
  }
}

static void BM_SVO_Set_With_RCU(benchmark::State& state) {
  SparseOctree<Voxel> svo;
  Palette             palette;

  palette.Create(Palette::Item {
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material {
           .Albedo = glm::vec4 {0.63f, 0.067f, 0.051f, 1.0f}})});

  auto     brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);
  uint64_t i     = 0;

  for (auto _ : state) {
    {
      auto     w = svo.BeginWrite();
      uint64_t x = i & 63;
      uint64_t y = (i >> 6) & 63;
      uint64_t z = (i >> 12) & 63;
      svo.SetWithoutCloneCheck(w, x, y, z, brick.get());
      i = (i + 1) & 262143;
    }
    svo.Sync();
  }
}

static void BM_SVO_Set_With_Copy(benchmark::State& state) {
  SparseOctreeWithoutRCU<Voxel> svo;
  Palette                       palette;

  palette.Create(Palette::Item {
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material {
           .Albedo = glm::vec4 {0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  uint64_t i = 0;

  for (auto _ : state) {
    uint64_t x = i & 63;
    uint64_t y = (i >> 6) & 63;
    uint64_t z = (i >> 12) & 63;
    svo.Set(x, y, z, brick.get());
    i = (i + 1) & 262143;
  }
}

static void BM_SVO_Set_Without_Copy(benchmark::State& state) {
  SparseOctreeWithoutRCU<Voxel> svo;
  Palette                       palette;

  palette.Create(Palette::Item {
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material {
           .Albedo = glm::vec4 {0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  uint64_t i = 0;

  for (auto _ : state) {
    uint64_t x = i & 63;
    uint64_t y = (i >> 6) & 63;
    uint64_t z = (i >> 12) & 63;
    svo.SetWithoutCopy(x, y, z, brick.get());
    i = (i + 1) & 262143;
  }
}

static void BM_SVO_Clear(benchmark::State& state) {
  SparseOctree<Voxel> svo;
  Palette             palette;

  palette.Create(Palette::Item {
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material {
           .Albedo = glm::vec4 {0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  {
    auto w = svo.BeginWrite();
    for (int x = 0; x < ITERATIONS; ++x)
      for (int y = 0; y < ITERATIONS; ++y)
        for (int z = 0; z < ITERATIONS; ++z)
          svo.Set(w, x, y, z, brick.get());
  }
  svo.Sync();

  for (auto _ : state) {
    auto w = svo.BeginWrite();
    for (int x = 0; x < ITERATIONS; ++x)
      for (int y = 0; y < ITERATIONS; ++y)
        for (int z = 0; z < ITERATIONS; ++z)
          svo.Clear(w, x, y, z);
  }
  svo.Sync();
}

static void BM_SVO_Get(benchmark::State& state) {
  SparseOctree<Voxel> svo;
  Palette             palette;

  palette.Create(Palette::Item {
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material {
           .Albedo = glm::vec4 {0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  {
    auto w = svo.BeginWrite();
    for (int x = 0; x < ITERATIONS; ++x)
      for (int y = 0; y < ITERATIONS; ++y)
        for (int z = 0; z < ITERATIONS; ++z)
          svo.Set(w, x, y, z, brick.get());
  }
  svo.Sync();

  for (auto _ : state) {
    auto w = svo.BeginRead();
    for (int x = 0; x < ITERATIONS; ++x)
      for (int y = 0; y < ITERATIONS; ++y)
        for (int z = 0; z < ITERATIONS; ++z)
          svo.Get(w, x, y, z);
  }
}

static void BM_SVO_Flatten(benchmark::State& state) {
  SparseOctree<Voxel> svo;
  Palette             palette;

  palette.Create(Palette::Item {
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material {
           .Albedo = glm::vec4 {0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  {
    auto w = svo.BeginWrite();
    for (int x = 0; x < 64; x += 2)
      for (int y = 0; y < 64; y += 2)
        for (int z = 0; z < 64; z += 2)
          svo.Set(w, x, y, z, brick.get());
  }
  svo.Sync();

  for (auto _ : state) {
    auto                                       w = svo.BeginRead();
    std::vector<SparseOctree<Voxel>::FlatNode> out;
    svo.Flatten(w, out);
  }
}

static void BM_SVO_Raymarch(benchmark::State& state) {
  SparseOctree<Voxel> svo;
  Palette             palette;

  palette.Create(Palette::Item {
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material {
           .Albedo = glm::vec4 {0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  {
    auto w = svo.BeginWrite();
    for (int x = 0; x < 64; x += 2)
      for (int y = 0; y < 64; y += 2)
        for (int z = 0; z < 64; z += 2)
          svo.Set(w, x, y, z, brick.get());
  }
  svo.Sync();

  struct Ray {
    glm::vec3 origin;
    glm::vec3 dir;
  };

  std::vector<Ray> rays;
  glm::vec3        camera = {32.f, 32.f, 144.f};

  const int   resolution = 128;
  const float volumeSize = 64.0f;

  for (int y = 0; y < resolution; y++)
    for (int x = 0; x < resolution; x++) {

      float u = (x + 0.5f) / resolution;
      float v = (y + 0.5f) / resolution;

      glm::vec3 target {
          u * volumeSize,
          v * volumeSize,
          0.0f};

      glm::vec3 dir = glm::normalize(target - camera);

      rays.push_back({camera, dir});
    }

  std::shuffle(rays.begin(), rays.end(), std::mt19937(42));

  for (auto _ : state) {
    auto r = svo.BeginRead();

    for (const auto& ray : rays)
      svo.Raymarch2(r, ray.origin, ray.dir);
  }
}

static void BM_SVO_DeepRaymarch(benchmark::State& state) {
  SparseOctree<Voxel> svo;
  Palette             palette;

  palette.Create(Palette::Item {
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material {
           .Albedo = glm::vec4 {0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  {
    auto w = svo.BeginWrite();
    for (int x = 0; x < 64; x += 2)
      for (int y = 0; y < 64; y += 2)
        for (int z = 0; z < 64; z += 2)
          svo.Set(w, x, y, z, brick.get());
  }
  svo.Sync();

  for (auto _ : state) {
    auto w = svo.BeginRead();
    for (int x = 0; x < 64; x += 2)
      for (int y = 0; y < 64; y += 2)
        for (int z = 0; z < 64; z += 2)
          svo.DeepRaymarch2(w, {x, y, z}, {1, 0, 0});
  }
}

// static void BM_Buffer_Block_Allocate(benchmark::State& state) {

//   for (auto _ : state) {
//     akari::render::Buffer::Blocks blocks;
//     blocks.Allocate(0, 1024);
//     blocks.Allocate(0, 512);
//     blocks.Allocate(0, 1024);
//     blocks.Allocate(0, 256);
//     blocks.Allocate(0, 2048);
//   }
// }

// BENCHMARK(BM_SVO_Set_With_RCU_With_Copy_Check);
// BENCHMARK(BM_SVO_Set_With_RCU);
// BENCHMARK(BM_SVO_Set_With_Copy);
// BENCHMARK(BM_SVO_Set_Without_Copy);

// BENCHMARK(BM_SVO_Clear);
// BENCHMARK(BM_SVO_Get);
// BENCHMARK(BM_SVO_Flatten);
// BENCHMARK(BM_SVO_Raymarch);
// BENCHMARK(BM_SVO_DeepRaymarch);
// BENCHMARK(BM_Buffer_Block_Allocate);

// BENCHMARK_MAIN();

// int main(int argc, char** argv) {
//   akari::render::Buffer::Blocks blocks;

//   blocks.Allocate(0, 1024);
//   blocks.Allocate(0, 512);
//   blocks.Allocate(0, 256);
//   blocks.Allocate(0, 128);
//   blocks.Allocate(0, 2048);
//   blocks.Allocate(0, 1024);
//   blocks.Allocate(0, 2048);

//   std::cout << blocks.Id.size() << std::endl;
// }

// int main(int argc, char** argv) {
//   SparseOctreeWithoutRCU<Voxel> svo;
//   Palette             palette;

//   palette.Create(Palette::Item{
//       .Name = "Brick",
//       .Mat  = std::make_shared<Material>(Material{
//            .Albedo = glm::vec4{0.63f, 0.067f, 0.051f, 1.0f}})});

//   auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

//   {
//     for (int x = 0; x < ITERATIONS; ++x)
//       for (int y = 0; y < ITERATIONS; ++y)
//         for (int z = 0; z < ITERATIONS; ++z)
//           svo.SetWithoutCopy(x, y, z, brick.get());
//   }

//   return EXIT_SUCCESS;
// }

int main(int argc, char** argv) {
  SparseOctree<Voxel> svo;
  Palette             palette;

  palette.Create(Palette::Item{
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material{
           .Albedo = glm::vec4{0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  {
    auto w = svo.BeginWrite();
    for (int x = 0; x < 64; ++x)
      for (int y = 0; y < 64; ++y)
        for (int z = 0; z < 64; ++z)
          svo.Set(w, x, y, z, brick.get());
  }
  svo.Sync();

  return EXIT_SUCCESS;
}