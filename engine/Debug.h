#pragma once

#ifdef DEBUG

#include <bitset>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/glm.hpp>
#include <immintrin.h>
#include <iostream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

template <typename... Args>
inline void Log(const char* file, int line, const char* functionName, const Args&... args) {
  std::cout << "LOG " << file << ":" << line << " (" << functionName << "):";
  ((std::cout << " " << args), ...);
  std::cout << std::endl;
}

inline void LogIVec3(const char* file, int line, const char* functionName, const std::string& name, const glm::ivec3& position) {

  Log(file, line, functionName, name, "(", position.x, ",", position.y, ",", position.z, ")");
}

inline void LogVec3(const char* file, int line, const char* functionName, const std::string& name, const glm::vec3& position) {

  Log(file, line, functionName, name, "(", position.x, ",", position.y, ",", position.z, ")");
}

inline void LogVec4(const char* file, int line, const char* functionName, const std::string& name, const glm::vec4& position) {

  Log(file, line, functionName, name, "(", position.x, ",", position.y, ",", position.z, ",", position.w, ")");
}

template <typename... Args>
inline void LogToFile(const char* file, int line, const char* functionName, const std::string& outputFile, const Args&... args) {
  const std::string logDir = "logs/";
  fs::create_directories(logDir);

  std::ofstream ofs(logDir + outputFile, std::ios::app);
  if (!ofs.is_open())
    return;

  ofs << "LOG " << file << ":" << line << " (" << functionName << "):";
  ((ofs << " " << args), ...);
  ofs << std::endl;
}

inline void Benchmark(const char* file, int line, const char* functionName, const std::function<void()>& func, int iterations) {

  for (int i = 0; i < 50; ++i)
    func();

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < iterations; ++i)
    func();

  auto                                      end     = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> elapsed = end - start;

  Log(file, line, functionName, "Took:", elapsed.count() / iterations, "ms (average) over", iterations, "iterations");
}

#define LOG(...) Log(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define BENCHMARK(...) Benchmark(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_IVEC3(...) LogIVec3(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_VEC3(...) LogVec3(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_VEC4(...) LogVec4(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_TO_FILE(outputFile, ...) LogToFile(__FILE__, __LINE__, __func__, outputFile, __VA_ARGS__)

#else
#define LOG(...)
#define BENCHMARK(...)
#define LOG_IVEC3(...)
#define LOG_VEC3(...)
#define LOG_VEC4(...)
#define LOG_TO_FILE(...)
#endif