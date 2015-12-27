/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2015 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/bit_map.h"

#include "xenia/base/assert.h"
#include "xenia/base/atomic.h"
#include "xenia/base/math.h"

namespace xe {

BitMap::BitMap() = default;

BitMap::BitMap(size_t size) { Resize(size); }

BitMap::BitMap(uint32_t* data, size_t size) {
  assert_true(size % 4 == 0);

  data_.resize(size / 4);
  std::memcpy(data_.data(), data, size);
}

size_t BitMap::Acquire() {
  for (size_t i = 0; i < data_.size(); i++) {
    uint32_t entry = 0;
    uint32_t new_entry = 0;
    int acquired_idx = -1;

    do {
      entry = data_[i];
      uint8_t index = lzcnt(entry);
      if (index == kDataSizeBits) {
        // None free.
        acquired_idx = -1;
        break;
      }

      // Entry has a free bit. Acquire it.
      uint32_t bit = 1 << (kDataSizeBits - index - 1);
      new_entry = entry & ~bit;
      assert_not_zero(entry & bit);

      acquired_idx = index;
    } while (!atomic_cas(entry, new_entry, &data_[i]));

    if (acquired_idx != -1) {
      // Acquired.
      return (i * kDataSizeBits) + acquired_idx;
    }
  }

  return -1;
}

void BitMap::Release(size_t index) {
  auto slot = index / kDataSizeBits;
  index -= slot * kDataSizeBits;

  uint32_t bit = 1 << (kDataSizeBits - index - 1);

  uint32_t entry = 0;
  uint32_t new_entry = 0;
  do {
    entry = data_[slot];
    assert_zero(entry & bit);

    new_entry = entry | bit;
  } while (!atomic_cas(entry, new_entry, &data_[slot]));
}

void BitMap::Resize(size_t new_size) {
  auto old_size = data_.size();
  assert_true(new_size % 4 == 0);
  data_.resize(new_size / 4);

  // Initialize new entries.
  if (data_.size() > old_size) {
    for (size_t i = old_size; i < data_.size(); i++) {
      data_[i] = -1;
    }
  }
}

void BitMap::Reset() {
  for (size_t i = 0; i < data_.size(); i++) {
    data_[i] = -1;
  }
}

}  // namespace xe