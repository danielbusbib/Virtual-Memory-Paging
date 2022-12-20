#include "VirtualMemory.h"
#include "PhysicalMemory.h"

void fill_table (uint64_t frame)
{
  for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
      PMwrite ((frame * PAGE_SIZE) + i, 0);
    }
}

/*
 * Initialize the virtual memory.
 */
void VMinitialize ()
{
  fill_table (0);
}

uint64_t is_empty_frame (uint64_t num_frame)
{
  if (num_frame >= NUM_FRAMES) return 0;
  word_t val;
  for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
      PMread (num_frame * PAGE_SIZE + i, &val);
      if (val != 0) return 0;
    }
  return 1;
}

uint64_t min (uint64_t v1, uint64_t v2)
{
  return v1 < v2 ? v1 : v2;
}

uint64_t abs (uint64_t v)
{
  return v < 0 ? -v : v;
}

int is_in_not_evict_frames (const uint64_t *frames, uint64_t p)
{
  for (uint64_t i = 0; i < TABLES_DEPTH; ++i)
    {
      if (frames[i] == p) return 1;
    }
  return 0;
}
// DFS
void find_unused_frame (uint64_t &curr_frame, uint64_t &max_seen, uint64_t depth,
                        uint64_t &frame_empty, uint64_t *forbidden, uint64_t &prev,
                        uint64_t &frame_empty_parent, uint64_t &max_val_page, uint64_t &max_val,
                        uint64_t page_swapped, uint64_t &parent_max_addr, uint64_t &curr_evicted, uint64_t curr_page)
{
  if (curr_frame > max_seen)
    {
      max_seen = curr_frame;
    }
  if (depth == TABLES_DEPTH)
    {
      uint64_t curr_cyclic = min (NUM_PAGES - abs (page_swapped - curr_page), abs (page_swapped - curr_page));
      // c
      if (curr_cyclic > max_val)
        {
          parent_max_addr = prev;
          max_val = curr_cyclic;
          curr_evicted = curr_frame; // frame
          max_val_page = curr_page; // p
        }
      return;
    } // end
  if (!frame_empty && is_empty_frame (curr_frame) && !is_in_not_evict_frames (forbidden, curr_frame))
    {
      frame_empty = curr_frame;
      frame_empty_parent = prev;
    }
  int val = 0;
  // recursion
  for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
      PMread ((curr_frame * PAGE_SIZE) + i, &val);
      if (val != 0)
        {
          uint64_t old1 = curr_frame, old2 = prev;
          prev = curr_frame * PAGE_SIZE + i;
          curr_frame = val;
          find_unused_frame (curr_frame, max_seen, depth + 1,
                             frame_empty, forbidden,
                             prev, frame_empty_parent,
                             max_val_page, max_val, page_swapped,
                             parent_max_addr, curr_evicted,
                             (curr_page << OFFSET_WIDTH) + i);
          curr_frame = old1;
          prev = old2;
        }
    }
}

uint64_t physical_address (uint64_t virtualAddress)
{
  uint64_t offset = virtualAddress % PAGE_SIZE, frame, prev = 0;
  word_t addr1 = 0;
  uint64_t frames[TABLES_DEPTH] = {};
  uint64_t pageNum = virtualAddress >> OFFSET_WIDTH;
  for (int i = 0; i < TABLES_DEPTH; ++i)
    {
      uint64_t max_seen = 0, max_page = 0, max_val_cyclic = 0, runningParent = 0,
          parent_max = 0, empty_frame = 0, frame_to_evict = 0, parent_empty_frame = 0;
      uint64_t suffix = (virtualAddress >> (TABLES_DEPTH - i) * OFFSET_WIDTH) % PAGE_SIZE;
      frame = 0;
      PMread ((prev * PAGE_SIZE) + suffix, &addr1);
      if (addr1 == 0)
        {
          find_unused_frame (frame, max_seen, 0, empty_frame, frames, runningParent,
                             parent_empty_frame, max_page, max_val_cyclic, pageNum, parent_max,
                             frame_to_evict, 0);

          if (empty_frame != 0)
            {
              PMwrite (parent_empty_frame, 0); // remove reference from parent
              frame = empty_frame;
            }
          else if (max_seen + 1 < NUM_FRAMES)
            {
              frame = max_seen + 1;
            }
          else
            {
              PMevict (frame_to_evict, max_page);
              PMwrite (parent_max, 0);
              frame = frame_to_evict;
            }
          if (i < TABLES_DEPTH - 1) fill_table (frame);
          PMwrite ((prev * PAGE_SIZE) + suffix, (word_t) frame);
          addr1 = (word_t) frame;
        }
      frames[i] = addr1;
      prev = addr1;
    }
  // last level
  PMrestore (addr1, pageNum);
  return (addr1 * PAGE_SIZE) + offset;
}
/* Reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread (uint64_t virtualAddress, word_t *value)
{
  if (virtualAddress >= VIRTUAL_MEMORY_SIZE)
    return 0;
  PMread (physical_address (virtualAddress), value);
  return 1;
}

/* Writes a word to the given virtual address.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite (uint64_t virtualAddress, word_t value)
{
  if (virtualAddress >= VIRTUAL_MEMORY_SIZE)
    return 0;
  PMwrite (physical_address (virtualAddress), value);
  return 1;
}
