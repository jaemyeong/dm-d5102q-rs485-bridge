#pragma once
#include <stddef.h>
constexpr int MALLOC_CAP_8BIT = 1;
inline size_t& fakeFreeHeap() { static size_t value = 160000; return value; }
inline size_t& fakeLargestBlock() { static size_t value = 90000; return value; }
inline size_t heap_caps_get_free_size(int) { return fakeFreeHeap(); }
inline size_t heap_caps_get_largest_free_block(int) { return fakeLargestBlock(); }
