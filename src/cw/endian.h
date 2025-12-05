#pragma once

#include <bit>
#include <vector>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace cw {

	namespace binary {

		template <typename T>
		concept Integer = std::is_integral_v<T>;

		template<Integer T>
		constexpr T toBigEndian(T val) noexcept
		{
			if constexpr (std::endian::native == std::endian::little)
				return std::byteswap(val);

			else // bro is never gonna return :((
				return val;
			
		}

		template<Integer T>
		constexpr T fromBigEndian(T val) noexcept
		{
			if constexpr (std::endian::native == std::endian::little)
				return std::byteswap(val);

			else 
				return val;
			
		}

		template<Integer T>
		constexpr void writeBigEndian(uint8_t* dest,T littleEndianValue) noexcept
		{
			littleEndianValue = toBigEndian(littleEndianValue);
			std::memcpy(dest, &littleEndianValue, sizeof(T));
		}

		template<Integer T>
		void writeBigEndian(std::vector<uint8_t>& buffer, T value)
		{

			constexpr size_t bytes = sizeof(T);

			size_t oldSize = buffer.size();
				
			buffer.resize(oldSize + bytes);

				
			writeBigEndian(buffer.data() + oldSize, value);
		}
	



		template<Integer T>
		constexpr T readBigEndian(const uint8_t* src) noexcept
		{
			T v;
			std::memcpy(&v, src, sizeof(T));
			return fromBigEndian(v);
		}

	}

}