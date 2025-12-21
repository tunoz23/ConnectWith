#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

namespace cw::binary {

template <typename T>
concept Integral = std::is_integral_v<T>;

template <Integral T> [[nodiscard]] constexpr T toBigEndian(T value) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    return std::byteswap(value);
  } else {
    return value;
  }
}

template <Integral T>
[[nodiscard]] constexpr T fromBigEndian(T value) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    return std::byteswap(value);
  } else {
    return value;
  }
}

template <Integral T>
constexpr void writeBigEndian(uint8_t *dest, T value) noexcept {
  value = toBigEndian(value);
  std::memcpy(dest, &value, sizeof(T));
}

template <Integral T>
void writeBigEndian(std::vector<uint8_t> &buffer, T value) noexcept {
  const std::size_t oldSize = buffer.size();
  buffer.resize(oldSize + sizeof(T));
  writeBigEndian(buffer.data() + oldSize, value);
}

template <Integral T>
[[nodiscard]] constexpr T readBigEndian(const uint8_t *src) noexcept {
  T value;
  std::memcpy(&value, src, sizeof(T));
  return fromBigEndian(value);
}

template <Integral T>
[[nodiscard]] constexpr T
readBigEndian(std::span<const uint8_t> data) noexcept {
  return readBigEndian<T>(data.data());
}

} // namespace cw::binary
