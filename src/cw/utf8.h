#pragma once

#include <filesystem>
#include <string>

// UTF-8 path utilities for cross-platform file name handling.
// On Windows, std::filesystem::path uses wchar_t internally.
// On Linux/macOS, it uses char (UTF-8 native).
// These utilities ensure UTF-8 strings are correctly converted.

namespace cw::utf8 {

namespace fs = std::filesystem;

// Convert a filesystem path to a UTF-8 encoded std::string.
// Use this when sending file names over the network.
inline std::string pathToUtf8(const fs::path &p) {
#if defined(_WIN32)
  // On Windows, path::u8string() returns std::u8string in C++20,
  // but std::string in C++17. We use generic_u8string for forward slashes.
  auto u8str = p.generic_u8string();
  // In C++20, u8string is std::u8string, need to convert to std::string
  return std::string(reinterpret_cast<const char *>(u8str.data()),
                     u8str.size());
#else
  // On POSIX, paths are already UTF-8
  return p.generic_string();
#endif
}

// Convert a UTF-8 encoded std::string to a filesystem path.
// Use this when receiving file names from the network.
inline fs::path utf8ToPath(const std::string &utf8str) {
#if defined(_WIN32)
  // On Windows, construct path from u8string
  // C++17: path(u8"...") works directly
  // C++20: need to use std::u8string or path::operator/
  return fs::path(
      reinterpret_cast<const char8_t *>(utf8str.data()),
      reinterpret_cast<const char8_t *>(utf8str.data() + utf8str.size()));
#else
  // On POSIX, UTF-8 string works directly
  return fs::path(utf8str);
#endif
}

// Get the filename portion of a path as UTF-8 string.
inline std::string filenameToUtf8(const fs::path &p) {
  return pathToUtf8(p.filename());
}

} // namespace cw::utf8
