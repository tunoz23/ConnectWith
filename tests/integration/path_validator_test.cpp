// Path validator tests
// Uses real filesystem with temp directories

#include <filesystem>
#include <gtest/gtest.h>

#include "cw/file/path_validator.h"
#include "temp_directory.h"

using namespace cw::file;
namespace fs = std::filesystem;

class PathValidatorTest : public ::testing::Test {
protected:
  void SetUp() override {
    tempDir_ = std::make_unique<cw::test::TempDirectory>();
  }

  cw::test::TempDirectory &temp() { return *tempDir_; }
  const fs::path &baseDir() { return tempDir_->path(); }

  std::unique_ptr<cw::test::TempDirectory> tempDir_;
};

// ============================================================================
// Safe Paths
// ============================================================================

TEST_F(PathValidatorTest, SimpleFilename_IsSafe) {
  EXPECT_TRUE(isPathSafe(baseDir() / "test.txt", baseDir()));
}

TEST_F(PathValidatorTest, Subdirectory_IsSafe) {
  EXPECT_TRUE(isPathSafe(baseDir() / "subdir" / "test.txt", baseDir()));
}

TEST_F(PathValidatorTest, DeepNesting_IsSafe) {
  EXPECT_TRUE(
      isPathSafe(baseDir() / "a" / "b" / "c" / "d" / "e.txt", baseDir()));
}

TEST_F(PathValidatorTest, DotInFilename_IsSafe) {
  EXPECT_TRUE(isPathSafe(baseDir() / "file.name.with.dots.txt", baseDir()));
}

TEST_F(PathValidatorTest, RelativePath_IsSafe) {
  // Relative path prepended to base directory
  EXPECT_TRUE(isPathSafe(baseDir() / "test.txt", baseDir()));
}

// ============================================================================
// Unsafe Paths
// ============================================================================

TEST_F(PathValidatorTest, ParentTraversal_IsUnsafe) {
  EXPECT_FALSE(isPathSafe(baseDir() / ".." / "outside.txt", baseDir()));
}

TEST_F(PathValidatorTest, DeepParentTraversal_IsUnsafe) {
  EXPECT_FALSE(
      isPathSafe(baseDir() / ".." / ".." / ".." / "etc" / "passwd", baseDir()));
}

TEST_F(PathValidatorTest, HiddenParentTraversal_IsUnsafe) {
  // Go into subdir then back up past base
  EXPECT_FALSE(isPathSafe(baseDir() / "subdir" / ".." / ".." / "outside.txt",
                          baseDir()));
}

TEST_F(PathValidatorTest, AbsolutePathOutsideBase_IsUnsafe) {
  fs::path outsidePath = baseDir().parent_path() / "outside.txt";
  EXPECT_FALSE(isPathSafe(outsidePath, baseDir()));
}

TEST_F(PathValidatorTest, RootPath_IsUnsafe) {
  EXPECT_FALSE(isPathSafe(fs::path("/"), baseDir()));
}

#ifdef _WIN32
TEST_F(PathValidatorTest, AbsoluteWindowsPath_IsUnsafe) {
  EXPECT_FALSE(
      isPathSafe(fs::path("C:\\Windows\\System32\\config"), baseDir()));
}
#endif

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(PathValidatorTest, EmptyPath_Behavior) {
  // Empty path resolves to current directory
  // Just verify it doesn't crash
  fs::path empty;
  isPathSafe(empty, baseDir());
}

TEST_F(PathValidatorTest, DotPath_IsSafe) {
  EXPECT_TRUE(isPathSafe(baseDir() / ".", baseDir()));
}

TEST_F(PathValidatorTest, DotDotWithinSameDir_IsSafe) {
  // a/../b is equivalent to b
  EXPECT_TRUE(isPathSafe(baseDir() / "a" / ".." / "b.txt", baseDir()));
}
