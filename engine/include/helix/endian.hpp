#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

namespace helix {

// Explicit little-endian readers/writers. Host endianness is never assumed
// beyond memcpy of the converted integer.

inline void write_u8(std::vector<std::uint8_t>& out, std::uint8_t v) {
  out.push_back(v);
}

inline void write_u16le(std::vector<std::uint8_t>& out, std::uint16_t v) {
  out.push_back(static_cast<std::uint8_t>(v));
  out.push_back(static_cast<std::uint8_t>(v >> 8));
}

inline void write_u32le(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v));
  out.push_back(static_cast<std::uint8_t>(v >> 8));
  out.push_back(static_cast<std::uint8_t>(v >> 16));
  out.push_back(static_cast<std::uint8_t>(v >> 24));
}

inline void write_u64le(std::vector<std::uint8_t>& out, std::uint64_t v) {
  write_u32le(out, static_cast<std::uint32_t>(v));
  write_u32le(out, static_cast<std::uint32_t>(v >> 32));
}

inline void write_i64le(std::vector<std::uint8_t>& out, std::int64_t v) {
  write_u64le(out, static_cast<std::uint64_t>(v));
}

class ByteReader {
 public:
  explicit ByteReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

  [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - off_; }
  [[nodiscard]] std::size_t offset() const noexcept { return off_; }

  std::uint8_t u8() {
    require(1);
    return bytes_[off_++];
  }

  std::uint16_t u16le() {
    require(2);
    const std::uint16_t v = static_cast<std::uint16_t>(bytes_[off_]) |
                            static_cast<std::uint16_t>(bytes_[off_ + 1] << 8);
    off_ += 2;
    return v;
  }

  std::uint32_t u32le() {
    require(4);
    const std::uint32_t v = static_cast<std::uint32_t>(bytes_[off_]) |
                            (static_cast<std::uint32_t>(bytes_[off_ + 1]) << 8) |
                            (static_cast<std::uint32_t>(bytes_[off_ + 2]) << 16) |
                            (static_cast<std::uint32_t>(bytes_[off_ + 3]) << 24);
    off_ += 4;
    return v;
  }

  std::uint64_t u64le() {
    const std::uint32_t lo = u32le();
    const std::uint32_t hi = u32le();
    return static_cast<std::uint64_t>(lo) | (static_cast<std::uint64_t>(hi) << 32);
  }

  std::int64_t i64le() { return static_cast<std::int64_t>(u64le()); }

  void expect_end() const {
    if (remaining() != 0) {
      throw std::runtime_error("trailing payload bytes");
    }
  }

 private:
  void require(std::size_t n) const {
    if (remaining() < n) {
      throw std::runtime_error("payload truncated");
    }
  }

  std::span<const std::uint8_t> bytes_;
  std::size_t off_{0};
};

}  // namespace helix
