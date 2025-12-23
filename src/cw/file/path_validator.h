#pragma once

#include <filesystem>

namespace cw::file {

namespace fs = std::filesystem;

// Validate that a path does not escape the base directory.
// Returns true if path is safe (within or equal to base).
// SECURITY: This function is critical for preventing path traversal attacks.
[[nodiscard]] inline bool isPathSafe(const fs::path &requestedPath,
                                     const fs::path &baseDir) {
  std::error_code ec;

  // Get canonical base (must exist)
  fs::path canonicalBase = fs::canonical(baseDir, ec);
  if (ec)
    return false;

  // Get absolute path of requested (may not exist yet)
  fs::path absoluteRequested = fs::absolute(requestedPath, ec);
  if (ec)
    return false;

  // Normalize to remove . and .. components
  fs::path normalized = absoluteRequested.lexically_normal();

  // Use lexically_relative to check if path is within base
  // If the path is outside base, lexically_relative returns a path
  // starting with ".." or an absolute path
  fs::path relative = normalized.lexically_relative(canonicalBase);

  if (relative.empty()) {
    return false;
  }

  // Check if relative path escapes (starts with "..")
  auto it = relative.begin();
  if (it != relative.end()) {
    std::string first = it->string();
    if (first == "..") {
      return false;
    }
  }

  // Also reject if the relative path is itself absolute (different root)
  if (relative.is_absolute()) {
    return false;
  }

  return true;
}

} // namespace cw::file
