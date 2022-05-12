/*
 * I/O Advanced Programmable Interrupt Controller (IOAPIC)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2017-2018 Markus Partheymüller, Cyberus Technology GmbH.
 *
 * This file is part of the Hedron hypervisor.
 *
 * Hedron is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hedron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#pragma once

#include "algorithm.hpp"
#include "hip.hpp"
#include "list.hpp"
#include "static_vector.hpp"

// A single IOAPIC
//
// All public functions of this class are safe to be called without synchronization, unless stated otherwise
// in its documentation.
class Ioapic : public Forward_list<Ioapic>
{
private:
    uint32 const paddr;
    mword const reg_base;
    unsigned const gsi_base;
    unsigned const id;
    uint16 rid;
    Spinlock lock;

    static Ioapic* list;

    enum
    {
        IOAPIC_IDX = 0x0,
        IOAPIC_WND = 0x10,
        IOAPIC_PAR = 0x20,
        IOAPIC_EOI = 0x40,

        // The maximum number of redirection table entries allowed by the
        // specification.
        IOAPIC_MAX_IRT = 0xf0,
    };

    enum Register
    {
        IOAPIC_ID = 0x0,
        IOAPIC_VER = 0x1,
        IOAPIC_ARB = 0x2,
        IOAPIC_BCFG = 0x3,
        IOAPIC_IRT = 0x10,
    };

    enum Irt_entry : uint64
    {
        // Constants for remappable (IOMMU) IRT entries.
        IRT_REMAPPABLE_HANDLE_15_SHIFT = 11,
        IRT_REMAPPABLE_HANDLE_0_14_SHIFT = 49,
        IRT_FORMAT_REMAPPABLE = 1UL << 48,

        // Constants for legacy (non-IOMMU) IRT entries.
        IRT_DESTINATION_SHIFT = 56,

        IRT_MASKED = 1UL << 16,
        IRT_TRIGGER_MODE_LEVEL = 1UL << 15,
        IRT_POLARITY_ACTIVE_LOW = 1UL << 13,
    };

    // A shadow copy of the IRT.
    //
    // Each write to the IRT must also be performed here. This saves us from expensive reads of the IRT when
    // we want to mask an IRT entry.
    //
    // One bit that is not correctly shadowed is the 'Remote IRR' bit. We never set it ourselves, so we will
    // always overwrite it with zero. This should be harmless.
    Static_vector<uint64, IOAPIC_MAX_IRT> shadow_redir_table;

    inline void index(Register reg) { *reinterpret_cast<uint8 volatile*>(reg_base + IOAPIC_IDX) = reg; }

    inline uint32 read(Register reg)
    {
        index(reg);
        return *reinterpret_cast<uint32 volatile*>(reg_base + IOAPIC_WND);
    }

    inline void write(Register reg, uint32 val)
    {
        index(reg);
        *reinterpret_cast<uint32 volatile*>(reg_base + IOAPIC_WND) = val;
    }

    inline uint32 read_id_reg() { return read(IOAPIC_ID); }
    inline uint32 read_version_reg() { return read(IOAPIC_VER); }
    inline uint32 get_paddr() { return paddr; }

    inline unsigned get_gsi() const { return gsi_base; }

    inline unsigned version() { return read(IOAPIC_VER) & 0xff; }

    // Write an IRT entry to the IOAPIC without caching it.
    //
    // This has to be used with caution, because we rely on shadow_redir_table to reflect the actual state of
    // the IRT.
    void set_irt_entry_uncached(size_t entry, uint64 val);

    // A special case of set_irt_entry_uncached that only sets the low 32-bit of the IRT entry.
    //
    // This uses 2 instead of 4 MMIO operations and should be significantly faster. This is useful, because
    // the low 32-bit contain the frequently toggled mask bit.
    void set_irt_entry_uncached_low(size_t entry, uint32 val);

    // Write an IRT entry and update the write-through cache.
    //
    // This function may not re-write the full IRT entry, if the written value is not changed.
    void set_irt_entry(size_t entry, uint64 val);

    // Read an IRT entry from the write-through cache.
    uint64 get_irt_entry(size_t entry) const;

    // Restore the IOAPIC state from shadow_redir_table.
    void restore();

public:
    Ioapic(Paddr paddr_, unsigned id_, unsigned gsi_base_);

    static void* operator new(size_t);

    static inline bool claim_dev(unsigned r, unsigned i)
    {
        auto range = Forward_list_range(list);
        auto it = find_if(range, [i](auto const& ioapic) { return ioapic.rid == 0 and ioapic.id == i; });

        if (it != range.end()) {
            it->rid = static_cast<uint16>(r);
            return true;
        } else {
            return false;
        }
    }

    static inline void add_to_hip(Hip_ioapic*& entry)
    {
        for (auto& ioapic : Forward_list_range(list)) {
            entry->id = ioapic.read_id_reg();
            entry->version = ioapic.read_version_reg();
            entry->gsi_base = ioapic.get_gsi();
            entry->base = ioapic.get_paddr();
            entry++;
        }
    }

    // Return the highest entry index (pin number).
    //
    // The returned value is one less then the count of pins.
    inline unsigned irt_max() { return read(IOAPIC_VER) >> 16 & 0xff; }

    inline uint16 get_rid() const { return rid; }

    // Configure an IRT entry (IOMMU disabled).
    //
    // The IRT entry will be unmasked after this call.
    //
    // For context, see Section 5.1.2.1 "Interrupts in Compatibility Format" in the the VT-d specification.
    //
    // This will only configure a working interrupt, if the IOMMU is not configured for Interrupt
    // Remapping. Use set_irt_entry_remappable instead, when Interupt Remapping is enabled.
    void set_irt_entry_compatibility(unsigned gsi, unsigned apic_id, unsigned vector, bool level,
                                     bool active_low);

    // Configure an IRT entry (IOMMU enabled).
    //
    // The IRT entry will be unmasked after this call.
    //
    // For context, see Section 5.1.2.2 "Interrupts in Remappable Format" in the the VT-d specification.
    void set_irt_entry_remappable(unsigned gsi, unsigned iommu_irt_index, unsigned vector, bool level,
                                  bool active_low);

    // Prepare all IOAPICs in the system for system suspend by saving their state to memory.
    static void save_all();

    // Restore the state of all IOAPICs from memory after a system resume.
    static void restore_all();
};
