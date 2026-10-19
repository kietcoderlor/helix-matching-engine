#pragma once

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace helix {

// Bounded wait-free SPSC ring. Exactly one thread may call try_push,
// exactly one thread may call try_pop. Capacity must be a power of two.
// A failed try_push does not overwrite; the caller applies backpressure.
template <typename T>
class SpscQueue {
 public:
  explicit SpscQueue(std::size_t capacity)
      : buf_(round_up_pow2(capacity)), mask_(buf_.size() - 1) {
    if (buf_.size() < 2) {
      throw std::invalid_argument("SpscQueue capacity must be at least 2");
    }
  }

  [[nodiscard]] std::size_t capacity() const noexcept { return buf_.size(); }

  // Producer. Returns false when full. Moves `value` only on success.
  bool try_push(T&& value) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    const std::size_t next = tail + 1;
    const std::size_t head = head_.load(std::memory_order_acquire);
    if ((next - head) > buf_.size()) {
      return false;
    }
    buf_[tail & mask_] = std::move(value);
    tail_.store(next, std::memory_order_release);
    return true;
  }

  // Consumer. Returns false when empty.
  bool try_pop(T& out) {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    if (head == tail) {
      return false;
    }
    out = std::move(buf_[head & mask_]);
    head_.store(head + 1, std::memory_order_release);
    return true;
  }

  [[nodiscard]] bool empty() const noexcept {
    return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
  }

 private:
  static std::size_t round_up_pow2(std::size_t n) {
    std::size_t c = 1;
    while (c < n) {
      c <<= 1;
    }
    return c;
  }

  std::vector<T> buf_;
  std::size_t mask_;
  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
};

}  // namespace helix
