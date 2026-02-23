#include <benchmark/benchmark.h>
#include <vector>

#include "voxel/Palette.h"
#include "voxel/SparseOctree.h"
#include "voxel/SparseOctreeWithoutRCU.h"
#include "voxel/Voxel.h"

static void BM_SVO_Set_With_RCU(benchmark::State& state) {
  SparseOctree<Voxel> svo;
  Palette             palette;

  palette.Create(Palette::Item{
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material{
           .Albedo = glm::vec4{0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  for (auto _ : state) {
    auto w = svo.BeginWrite();
    svo.Set(w, 0, 0, 0, brick.get());
  }

  svo.Sync();
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
    svo.Set(0, 0, 0, brick.get());
}


static void BM_SVO_Set_With_Copy_With_Atomic_Root(benchmark::State& state) {
  SparseOctreeWithoutRCU<Voxel> svo;
  Palette                       palette;

  palette.Create(Palette::Item{
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material{
           .Albedo = glm::vec4{0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  for (auto _ : state)
    svo.SetWithCopyWithAtomicRoot(0, 0, 0, brick.get());
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
    svo.SetWithoutCopy(0, 0, 0, brick.get());
}

static void BM_SVO_Set_Without_Copy_With_Atomic_Root(benchmark::State& state) {
  SparseOctreeWithoutRCU<Voxel> svo;
  Palette                       palette;

  palette.Create(Palette::Item{
      .Name = "Brick",
      .Mat  = std::make_shared<Material>(Material{
           .Albedo = glm::vec4{0.63f, 0.067f, 0.051f, 1.0f}})});

  auto brick = std::make_shared<Voxel>(palette.Find("Brick")->Id);

  for (auto _ : state)
    svo.SetWithoutCopyWithAtomicRoot(0, 0, 0, brick.get());
}

BENCHMARK(BM_SVO_Set_With_RCU);
BENCHMARK(BM_SVO_Set_With_Copy);
BENCHMARK(BM_SVO_Set_With_Copy_With_Atomic_Root);
BENCHMARK(BM_SVO_Set_Without_Copy);
BENCHMARK(BM_SVO_Set_Without_Copy_With_Atomic_Root);

BENCHMARK_MAIN();