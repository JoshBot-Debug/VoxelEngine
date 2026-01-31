#include "HeightMap.h"
#include <chrono>

HeightMap::HeightMap(int width, int height, const TerrainSpecification& specification)
    : Terrain(specification) {
  Initialize(width, height);
}

void HeightMap::Initialize(int width, int height) {
  m_Width  = width;
  m_Height = height;

  if (Terrain.Seed == 0)
    m_Perlin.SetSeed(static_cast<int>(std::time(0)));
  else
    m_Perlin.SetSeed(Terrain.Seed);
  m_Perlin.SetFrequency(Terrain.Frequency);
  m_Perlin.SetPersistence(Terrain.Persistence);
  m_Perlin.SetOctaveCount(Terrain.OctaveCount);

  m_ScaleBias.SetSourceModule(0, m_Perlin);
  m_ScaleBias.SetScale(Terrain.Scale);
  m_ScaleBias.SetBias(Terrain.Bias);
}

utils::NoiseMap HeightMap::Build(double lowerXBound, double upperXBound, double lowerZBound, double upperZBound) {
  utils::NoiseMap             heightMap;
  utils::NoiseMapBuilderPlane heightMapBuilder;

  heightMapBuilder.SetSourceModule(m_ScaleBias);
  heightMapBuilder.SetDestNoiseMap(heightMap);
  heightMapBuilder.SetDestSize(m_Width, m_Height);
  heightMapBuilder.SetBounds(lowerXBound, upperXBound, lowerZBound, upperZBound);
  heightMapBuilder.Build();

  return heightMap;
}