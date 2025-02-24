/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vmem.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>
#include <random>

#include "champsim.h"
#include "util.h"

#if (USER_CODES == ENABLE)
// Note ppage_free_list won't be pushed, which means ChampSim won't handle the case when the physical memory is full.

VirtualMemory::VirtualMemory(uint64_t capacity, uint64_t pg_size, uint32_t page_table_levels, uint64_t random_seed, uint64_t minor_fault_penalty)
  : minor_fault_penalty(minor_fault_penalty), pt_levels(page_table_levels), page_size(pg_size),
  ppage_free_list((capacity - VMEM_RESERVE_CAPACITY) / PAGE_SIZE, PAGE_SIZE)
{
  assert(capacity % PAGE_SIZE == 0);
  assert(pg_size == (1ul << lg2(pg_size)) && pg_size > 1024);

  // populate the free list
  ppage_free_list.front() = VMEM_RESERVE_CAPACITY;

  // Fill ppage_free_list with a sequence that begins as ppage_free_list.front() and the increment is PAGE_SIZE.
  // Note the initial size of ppage_free_list is (capacity - VMEM_RESERVE_CAPACITY).
  std::partial_sum(std::cbegin(ppage_free_list), std::cend(ppage_free_list), std::begin(ppage_free_list));

  //cout << ppage_free_list.front() << " " << ppage_free_list.back() << endl;

  // then shuffle it
  std::shuffle(std::begin(ppage_free_list), std::end(ppage_free_list), std::mt19937_64{random_seed});

  next_pte_page = ppage_free_list.front();
  ppage_free_list.pop_front();
}

// Calculate the shift amount of virtual address for accessing the page table
uint64_t VirtualMemory::shamt(uint32_t level) const { return LOG2_PAGE_SIZE + lg2(page_size / PTE_BYTES) * (level); }

// Calculate the offset in the page table for a virtual address
uint64_t VirtualMemory::get_offset(uint64_t vaddr, uint32_t level) const { return (vaddr >> shamt(level)) & bitmask(lg2(page_size / PTE_BYTES)); }

std::pair<uint64_t, bool> VirtualMemory::va_to_pa(uint32_t cpu_num, uint64_t vaddr)
{
  auto [ppage, fault] = vpage_to_ppage_map.insert({{cpu_num, vaddr >> LOG2_PAGE_SIZE}, ppage_free_list.front()});

  // this vpage doesn't yet have a ppage mapping
  if (fault)
  {
    ppage_free_list.pop_front();
#if (PRINT_STATISTICS_INTO_FILE == ENABLE)
    output_statistics.virtual_page_count++;
#endif  // PRINT_STATISTICS_INTO_FILE
  }

  return {splice_bits(ppage->second, vaddr, LOG2_PAGE_SIZE), fault};
}

std::pair<uint64_t, bool> VirtualMemory::get_pte_pa(uint32_t cpu_num, uint64_t vaddr, uint32_t level)
{
  std::tuple key{cpu_num, vaddr >> shamt(level + 1), level};
  auto [ppage, fault] = page_table.insert({key, next_pte_page});

  // this PTE doesn't yet have a mapping
  if (fault)
  {
    next_pte_page = ppage_free_list.front();
    ppage_free_list.pop_front();

#if (PRINT_STATISTICS_INTO_FILE == ENABLE)
    output_statistics.valid_pte_count[level - 1]++;
#endif  // PRINT_STATISTICS_INTO_FILE
  }

  return {splice_bits(ppage->second, get_offset(vaddr, level) * PTE_BYTES, lg2(page_size)), fault};
}
#else
VirtualMemory::VirtualMemory(uint64_t capacity, uint64_t pg_size, uint32_t page_table_levels, uint64_t random_seed, uint64_t minor_fault_penalty)
  : minor_fault_penalty(minor_fault_penalty), pt_levels(page_table_levels), page_size(pg_size),
  ppage_free_list((capacity - VMEM_RESERVE_CAPACITY) / PAGE_SIZE, PAGE_SIZE)
{
  assert(capacity % PAGE_SIZE == 0);
  assert(pg_size == (1ul << lg2(pg_size)) && pg_size > 1024);

  // populate the free list
  ppage_free_list.front() = VMEM_RESERVE_CAPACITY;

  std::partial_sum(std::cbegin(ppage_free_list), std::cend(ppage_free_list), std::begin(ppage_free_list));

  // then shuffle it
  std::shuffle(std::begin(ppage_free_list), std::end(ppage_free_list), std::mt19937_64{random_seed});

  next_pte_page = ppage_free_list.front();
  ppage_free_list.pop_front();
}

uint64_t VirtualMemory::shamt(uint32_t level) const { return LOG2_PAGE_SIZE + lg2(page_size / PTE_BYTES) * (level); }

uint64_t VirtualMemory::get_offset(uint64_t vaddr, uint32_t level) const { return (vaddr >> shamt(level)) & bitmask(lg2(page_size / PTE_BYTES)); }

std::pair<uint64_t, bool> VirtualMemory::va_to_pa(uint32_t cpu_num, uint64_t vaddr)
{
  auto [ppage, fault] = vpage_to_ppage_map.insert({{cpu_num, vaddr >> LOG2_PAGE_SIZE}, ppage_free_list.front()});

  // this vpage doesn't yet have a ppage mapping
  if (fault)
    ppage_free_list.pop_front();

  return {splice_bits(ppage->second, vaddr, LOG2_PAGE_SIZE), fault};
}

std::pair<uint64_t, bool> VirtualMemory::get_pte_pa(uint32_t cpu_num, uint64_t vaddr, uint32_t level)
{
  std::tuple key{cpu_num, vaddr >> shamt(level + 1), level};
  auto [ppage, fault] = page_table.insert({key, next_pte_page});

  // this PTE doesn't yet have a mapping
  if (fault)
  {
    next_pte_page += page_size;
    if (next_pte_page % PAGE_SIZE)
    {
      next_pte_page = ppage_free_list.front();
      ppage_free_list.pop_front();
    }
  }

  return {splice_bits(ppage->second, get_offset(vaddr, level) * PTE_BYTES, lg2(page_size)), fault};
}
#endif


