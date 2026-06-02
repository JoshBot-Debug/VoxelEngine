#pragma once

#include <noise/noise.h>
#include <noiseutils/noiseutils.h>

struct TerrainSpecification {
  int Seed {50};
  int OctaveCount {3};

  float Frequency {1.0f};
  float Persistence {0.4f};

  float Scale {0.6f};
  float Bias {-0.4f};
};

class HeightMap {
private:
  int m_Width;
  int m_Height;

  noise::module::Perlin    m_Perlin;
  noise::module::ScaleBias m_ScaleBias;

public:
  TerrainSpecification Terrain;

public:
  HeightMap() = default;
  HeightMap(int width, int height, const TerrainSpecification& specification);

  void Initialize(int width, int height);

  utils::NoiseMap Build(double lowerXBound, double upperXBound, double lowerZBound, double upperZBound);
};
