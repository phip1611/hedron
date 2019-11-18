/*
 * Host page table modification
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
 *
 * This file is part of the NOVA microhypervisor.
 *
 * NOVA is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOVA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#pragma once

#include "generic_page_table.hpp"
#include "page_table_policies.hpp"
#include "tlb_cleanup.hpp"

class Hpt_new;
using Hpt_page_table = Generic_page_table<9, mword, Atomic_access_policy<>, No_clflush_policy,
                                          Page_alloc_policy<>, Tlb_cleanup, Hpt_new>;

class Hpt_new : public Hpt_page_table
{
    private:

        // The number of leaf levels we support.
        static level_t supported_leaf_levels;

        static void flush()
        {
            mword cr3;
            asm volatile ("mov %%cr3, %0; mov %0, %%cr3" : "=&r" (cr3));
        }

    public:
        enum : pte_t {
            PTE_P  = 1ULL << 0,
            PTE_W  = 1ULL << 1,
            PTE_U  = 1ULL << 2,
            PTE_UC = 1ULL << 4,
            PTE_S  = 1ULL << 7,
            PTE_G  = 1ULL << 8,

            PTE_A  = 1ULL << 5,
            PTE_D  = 1ULL << 6,

            PTE_NX = 1ULL << 63,
        };

        enum : uint32 {
            ERR_W = 1U << 1,
            ERR_U = 1U << 2,
        };

        static constexpr pte_t mask {PTE_NX | 0xFFF};
        static constexpr pte_t all_rights {PTE_P | PTE_W | PTE_U | PTE_A | PTE_D};

        // Adjust the number of leaf levels to the given value.
        static void set_supported_leaf_levels(level_t level);

        // Return a structural copy of this page table for the given virtual
        // address range.
        Hpt_new deep_copy(mword vaddr_start, mword vaddr_end);

        void make_current (mword pcid)
        {
            mword phys_root {root()};

            assert ((phys_root & PAGE_MASK) == 0);
            asm volatile ("mov %0, %%cr3" : : "r" (phys_root | pcid) : "memory");
        }

        // Temporarily map the given physical memory.
        //
        // Establish a temporary mapping for the given physical address in a
        // special kernel virtual address region reserved for this
        // usecase. Return a pointer to access this memory. phys does not need
        // to be aligned.
        //
        // If use_boot_hpt is true, the mapping is established in the boot page
        // tables. If not, use the current Pd's kernel address space.
        //
        // The returned pointer is valid until the next remap call (on any core).
        static void *remap (Paddr phys, bool use_boot_hpt = true);

        // Atomically change a 4K page mapping to point to a new frame. Return
        // the old frame it pointed to.
        Paddr replace (mword vaddr, mword paddr);

        // Create a page table from existing page table structures.
        Hpt_new(pte_pointer_t rootp) : Hpt_page_table(4, supported_leaf_levels, rootp) {}

        // Create a page table from scratch.
        Hpt_new() : Hpt_page_table(4, supported_leaf_levels) {}

        // Convert mapping database attributes to page table attributes.
        static pte_t hw_attr(mword a);

        // The boot page table as constructed in start.S.
        static Hpt_new &boot_hpt();
};
