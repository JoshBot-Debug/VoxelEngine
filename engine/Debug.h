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

#define LOG(...) Log(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_IVEC3(...) LogIVec3(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_VEC3(...) LogVec3(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_VEC4(...) LogVec4(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_TO_FILE(outputFile, ...) LogToFile(__FILE__, __LINE__, __func__, outputFile, __VA_ARGS__)

#else
#define LOG(...)
#define LOG_IVEC3(...)
#define LOG_VEC3(...)
#define LOG_VEC4(...)
#define LOG_TO_FILE(...)
#endif