/*
 * SiFive E series machine interface
 *
 * Copyright (c) 2017 SiFive, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_MLTON_H
#define HW_MLTON_H

#include "hw/riscv/riscv_hart.h"

#define TYPE_RISCV_MLTON_SOC "riscv.mlton.soc"
#define RISCV_MLTON_SOC(obj) \
    OBJECT_CHECK(MLtonSoCState, (obj), TYPE_RISCV_MLTON_SOC)

typedef struct MLtonSoCState {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    RISCVHartArrayState cpus;
    DeviceState *plic;

    MemoryRegion mask_rom;

} MLtonSoCState;

typedef struct MLtonState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MLtonSoCState soc;
} MLtonState;

#define TYPE_RISCV_MLTON_MACHINE MACHINE_TYPE_NAME("mlton")
#define RISCV_MLTON_MACHINE(obj) \
    OBJECT_CHECK(MLtonState, (obj), TYPE_RISCV_MLTON_MACHINE)

enum {
    MLTON_DEV_DEBUG,
    MLTON_DEV_MROM,
    MLTON_DEV_UART,
    MLTON_DEV_RAM
};

#endif
