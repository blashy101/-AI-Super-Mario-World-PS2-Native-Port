/* Adapted from snesrev/smw util.c; see licenses/snesrev.txt. */
#include "src/types.h"
MemBlk FindIndexInMemblk(MemBlk data, size_t i) {
  size_t end = data.size, left_off, right_off;
  if (end < 2)
    return (MemBlk) { 0, 0 };
  
  uint16 f = *(uint16 *)(data.ptr + (end -= 2));
  size_t mx = f & 0xfff;
  if (f & 0x4000) {
    size_t next_mx = *(uint16 *)(data.ptr + (end -= 2));
    if (next_mx >= 256) {
      if (i > mx || mx * 2 > end)
        return (MemBlk) { 0, 0 };
      end -= mx * 2 + 2;
      i = *(uint16 *)(data.ptr + end + i * 2);
    } else {
      if (i > mx || mx > end)
        return (MemBlk) { 0, 0 };
      end -= mx + 1;
      i = *(uint8 *)(data.ptr + end + i);
    }
    mx = next_mx;
  }
  if (f & 0x8000) {
    if (i > mx || mx * 2 > end)
      return (MemBlk) { 0, 0 };
    left_off = ((i == 0) ? mx * 2 : mx * 2 + *(uint16 *)(data.ptr + i * 2 - 2));
    right_off = (i == mx) ? end : mx * 2 + *(uint16 *)(data.ptr + i * 2);
  } else {
    if (i > mx || mx * 4 > end)
      return (MemBlk) { 0, 0 };
    left_off = ((i == 0) ? mx * 4 : mx * 4 + *(uint32 *)(data.ptr + i * 4 - 4));
    right_off = (i == mx) ? end : mx * 4 + *(uint32 *)(data.ptr + i * 4);
  }
  if (left_off > right_off || right_off > end)
    return (MemBlk) { 0, 0 };
  return (MemBlk) { data.ptr + left_off, right_off - left_off };
}

const uint8 *FindAddrInMemblk(MemBlk data, uint32 addr) {
  if (data.size < 2)
    return 0;  // sanity check
  // std::lower_bound
  int count = *(uint16 *)data.ptr, count_org = count;
  if (data.size < 2 + count * 6)
    return 0;  // sanity check
  const uint8 *first = data.ptr + 2;
  while (count) {
    const uint8 *it = first;
    int step = count >> 1;
    it += step * 3;
    if ((*(uint32 *)it & 0xffffff) < addr) {
      first = it + 3;
      count -= step + 1;
    } else {
      count = step;
    }
  }
  if (first >= data.ptr + 2 + count_org * 3 || (*(uint32 *)first & 0xffffff) != addr)
    first -= 3;
  uint32 src_off = *(uint32 *)first & 0xffffff;
  uint32 offset = *(uint32 *)(first + count_org * 3) & 0xffffff;
  offset += addr - src_off;
  if (offset >= data.size)
    return 0;
  return data.ptr + offset;
}
