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

  // Check that normalized path starts with the base directory
  auto [baseEnd, reqIt] =
      std::mismatch(canonicalBase.begin(), canonicalBase.end(),
                    normalized.begin(), normalized.end());

  // If we consumed all of canonicalBase, the path is within the base
  return baseEnd == canonicalBase.end();
}

} // namespace cw::file
