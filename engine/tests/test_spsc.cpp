#include "helix/spsc_queue.hpp"
#include "test_harness.hpp"

#include <thread>
#include <vector>

using namespace helix;

HELIX_TEST(spsc_push_pop_fifo) {
  SpscQueue<int> q(4);
  HELIX_CHECK(q.try_push(1));
  HELIX_CHECK(q.try_push(2));
  int v = 0;
  HELIX_CHECK(q.try_pop(v));
  HELIX_CHECK_EQ(v, 1);
  HELIX_CHECK(q.try_pop(v));
  HELIX_CHECK_EQ(v, 2);
  HELIX_CHECK(!q.try_pop(v));
}

HELIX_TEST(spsc_full_does_not_drop_or_move) {
  SpscQueue<int> q(2);  // rounds to 2, stores 2
  HELIX_CHECK(q.try_push(10));
  HELIX_CHECK(q.try_push(20));
  int extra = 30;
  HELIX_CHECK(!q.try_push(std::move(extra)));
  HELIX_CHECK_EQ(extra, 30);
  int v = 0;
  HELIX_CHECK(q.try_pop(v));
  HELIX_CHECK_EQ(v, 10);
  HELIX_CHECK(q.try_pop(v));
  HELIX_CHECK_EQ(v, 20);
}

HELIX_TEST(spsc_two_threads_transfer_all) {
  SpscQueue<int> q(1024);
  constexpr int n = 10000;
  std::thread prod([&] {
    for (int i = 0; i < n; ++i) {
      int x = i;
      while (!q.try_push(std::move(x))) {
        std::this_thread::yield();
      }
    }
  });
  std::vector<int> got;
  got.reserve(n);
  std::thread cons([&] {
    int v = 0;
    while (static_cast<int>(got.size()) < n) {
      if (q.try_pop(v)) {
        got.push_back(v);
      } else {
        std::this_thread::yield();
      }
    }
  });
  prod.join();
  cons.join();
  HELIX_CHECK_EQ(got.size(), static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    HELIX_CHECK_EQ(got[static_cast<std::size_t>(i)], i);
  }
}
