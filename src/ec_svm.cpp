/*
 * Execution Context (SVM)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * This file is part of the Hedron microhypervisor.
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

#include "ec.hpp"
#include "svm.hpp"

void Ec::svm_exception(mword reason)
{
    if (current()->regs.vmcb->exitintinfo & 0x80000000) {

        mword t = static_cast<mword>(current()->regs.vmcb->exitintinfo) >> 8 & 0x7;
        mword v = static_cast<mword>(current()->regs.vmcb->exitintinfo) & 0xff;

        if (t == 0 || (t == 3 && v != 3 && v != 4))
            current()->regs.vmcb->inj_control = current()->regs.vmcb->exitintinfo;
    }

    current()->regs.dst_portal = reason;
    send_msg<ret_user_vmrun>();
}

void Ec::handle_svm()
{
    current()->regs.vmcb->tlb_control = 0;
    Fpu::restore_xcr0();

    mword reason = static_cast<mword>(current()->regs.vmcb->exitcode);

    switch (reason) {
    case -1UL: // Invalid state
        reason = Vmcb::SVM_INVALID_STATE;
        break;
    case 0x400: // NPT
        reason = Vmcb::SVM_NPT_FAULT;
        break;
    }

    switch (reason) {

    case 0x40 ... 0x5f: // Exception
        svm_exception(reason);

    case 0x60: // EXTINT
        asm volatile("sti; nop; cli" : : : "memory");
        ret_user_vmrun();
    }

    current()->regs.dst_portal = reason;

    send_msg<ret_user_vmrun>();
}
