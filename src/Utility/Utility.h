#pragma once

#include <filesystem>
#include <string>
#include <unistd.h>
#include <limits.h>

inline std::string getExecutableDir() {
  char result[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
  return std::filesystem::path(std::string(result, (count > 0) ? static_cast<unsigned int>(count) : 0))
      .parent_path()
      .string();
}