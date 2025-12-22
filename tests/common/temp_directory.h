#pragma once

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace cw::test {

namespace fs = std::filesystem;

// RAII wrapper for temporary test directory.
// Creates a unique temp directory on construction, removes it on destruction.
// Thread-safe: Each instance creates its own unique directory.
class TempDirectory {
public:
  TempDirectory() {
    // Generate unique directory name
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000, 99999);

    std::string dirname = "cw_test_" + std::to_string(dis(gen));
    m_path = fs::temp_directory_path() / dirname;
    fs::create_directories(m_path);
  }

  ~TempDirectory() {
    std::error_code ec;
    fs::remove_all(m_path, ec);
    // Ignore errors on cleanup
  }

  // Non-copyable, non-movable (RAII resource)
  TempDirectory(const TempDirectory &) = delete;
  TempDirectory &operator=(const TempDirectory &) = delete;
  TempDirectory(TempDirectory &&) = delete;
  TempDirectory &operator=(TempDirectory &&) = delete;

  // Get the path to the temp directory
  [[nodiscard]] const fs::path &path() const noexcept { return m_path; }

  // Implicit conversion for convenience
  operator const fs::path &() const noexcept { return m_path; }

  // Create a file with given content
  void createFile(const std::string &name, const std::string &content) {
    std::ofstream f(m_path / name, std::ios::binary);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
  }

  // Check if a file exists
  [[nodiscard]] bool fileExists(const std::string &name) const {
    return fs::exists(m_path / name);
  }

  // Read file content
  [[nodiscard]] std::string readFile(const std::string &name) const {
    std::ifstream f(m_path / name, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
  }

private:
  fs::path m_path;
};

} // namespace cw::test
