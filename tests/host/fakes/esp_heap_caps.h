#pragma once
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
constexpr int MALLOC_CAP_8BIT = 1;
inline size_t& fakeFreeHeap() { static size_t value = 160000; return value; }
inline size_t& fakeLargestBlock() { static size_t value = 90000; return value; }
inline size_t heap_caps_get_minimum_free_size(int) { return 80000; }
inline size_t& fakeScratchLive() { static size_t value = 0; return value; }
inline size_t& fakeScratchPeak() { static size_t value = 0; return value; }
inline unsigned& fakeScratchCalls() { static unsigned value = 0; return value; }
inline unsigned& fakeScratchFailAt() { static unsigned value = 0; return value; }
inline void* heap_caps_malloc(size_t size, int) {
  ++fakeScratchCalls();
  if (fakeScratchCalls() == fakeScratchFailAt()) return nullptr;
  assert(fakeScratchLive() == 0); // Worker scratch lifetimes must never overlap.
  void* value = malloc(size);
  if (value) { fakeScratchLive() = size; if (size > fakeScratchPeak()) fakeScratchPeak() = size; }
  return value;
}
inline void heap_caps_free(void* value) { if (value) { assert(fakeScratchLive()); fakeScratchLive() = 0; free(value); } }
inline size_t& fakeFreeHeapCalls() { static size_t value = 0; return value; }
inline size_t& fakeLargestBlockCalls() { static size_t value = 0; return value; }
inline size_t heap_caps_get_free_size(int) { ++fakeFreeHeapCalls(); return fakeFreeHeap() - fakeScratchLive(); }
inline size_t heap_caps_get_largest_free_block(int) { ++fakeLargestBlockCalls(); return fakeLargestBlock(); }
