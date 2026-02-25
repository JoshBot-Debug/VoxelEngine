#include <benchmark/benchmark.h>
#include <vector>

#include "voxel/Palette.h"
#include "voxel/SparseOctree.h"
#include "voxel/SparseOctreeWithoutRCU.h"
#include "voxel/Voxel.h"

const int ITERATIONS = 16;

static void BM_SVO_Set_With_RCU_With_Clone_Check(benchmark::State& state) {
  SparseOctree<Voxel> svo;
  Palette             palette;

  palette.Create(Palette::Item{
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material{
           .Albedo = glm::vec4{0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  for (auto _ : state) {
    {
      auto w = svo.BeginWrite();
      for (int x = 0; x < ITERATIONS; ++x)
        for (int y = 0; y < ITERATIONS; ++y)
          for (int z = 0; z < ITERATIONS; ++z)
            svo.Set(w, x, y, z, brick.get());
    }
    svo.Sync();
  }
}

static void BM_SVO_Set_With_RCU(benchmark::State& state) {
  SparseOctree<Voxel> svo;
  Palette             palette;

  palette.Create(Palette::Item{
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material{
           .Albedo = glm::vec4{0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  for (auto _ : state) {
    {
      auto w = svo.BeginWrite();
      for (int x = 0; x < ITERATIONS; ++x)
        for (int y = 0; y < ITERATIONS; ++y)
          for (int z = 0; z < ITERATIONS; ++z)
            svo.SetWithoutCloneCheck(w, x, y, z, brick.get());
    }
    svo.Sync();
  }
}

static void BM_SVO_Set_With_Copy(benchmark::State& state) {
  SparseOctreeWithoutRCU<Voxel> svo;
  Palette                       palette;

  palette.Create(Palette::Item{
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material{
           .Albedo = glm::vec4{0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  for (auto _ : state)
    for (int x = 0; x < ITERATIONS; ++x)
      for (int y = 0; y < ITERATIONS; ++y)
        for (int z = 0; z < ITERATIONS; ++z)
          svo.Set(x, y, z, brick.get());
}

static void BM_SVO_Set_Without_Copy(benchmark::State& state) {
  SparseOctreeWithoutRCU<Voxel> svo;
  Palette                       palette;

  palette.Create(Palette::Item{
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material{
           .Albedo = glm::vec4{0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  for (auto _ : state)
    for (int x = 0; x < ITERATIONS; ++x)
      for (int y = 0; y < ITERATIONS; ++y)
        for (int z = 0; z < ITERATIONS; ++z)
          svo.SetWithoutCopy(x, y, z, brick.get());
}

BENCHMARK(BM_SVO_Set_With_RCU_With_Clone_Check);
BENCHMARK(BM_SVO_Set_With_RCU_With_Clone_Check);
BENCHMARK(BM_SVO_Set_With_RCU);
BENCHMARK(BM_SVO_Set_With_Copy);
BENCHMARK(BM_SVO_Set_Without_Copy);

BENCHMARK_MAIN();

// int main(int argc, char** argv) {
//   SparseOctreeWithoutRCU<Voxel> svo;
//   Palette             palette;

//   palette.Create(Palette::Item{
//       .Name = "Brick",
//       .Mat  = std::make_shared<Material>(Material{
//            .Albedo = glm::vec4{0.63f, 0.067f, 0.051f, 1.0f}})});

//   auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

//   {
//     // auto w = svo.BeginWrite();

//     for (int x = 0; x < 64; ++x)
//       for (int y = 0; y < 64; ++y)
//         for (int z = 0; z < 64; ++z)
//           // svo.SetWithoutCloneCheck(w, x, y, z, brick.get());
//           svo.SetWithoutCopy(x, y, z, brick.get());
//   }

//   // svo.Sync();

//   return EXIT_SUCCESS;
// }