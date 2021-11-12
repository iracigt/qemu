/*
 * QEMU RISC-V Test Board for MLton development
 *
 * Copyright (c) 2021 Grant Iraci 
 * 
 * 0) QEMU UART
 * 1) RAM
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

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/sysbus.h"
#include "hw/char/serial.h"
#include "hw/misc/unimp.h"
#include "target/riscv/cpu.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/mlton.h"
#include "hw/riscv/boot.h"
#include "chardev/char.h"
#include "sysemu/arch_init.h"
#include "sysemu/sysemu.h"

static const MemMapEntry mlton_memmap[] = {
    [MLTON_DEV_DEBUG] =         {        0x0,     0x1000 },
    [MLTON_DEV_MROM] =          {     0x1000,     0x2000 },
    [MLTON_DEV_UART] =          { 0x10000000,      0x100 },
    [MLTON_DEV_RAM] =           { 0x80000000,   0x600000 },
};

static void mlton_machine_init(MachineState *machine)
{
    const MemMapEntry *memmap = mlton_memmap;

    MLtonState *s = RISCV_MLTON_MACHINE(machine);
    MemoryRegion *sys_mem = get_system_memory();
    MemoryRegion *main_mem = g_new(MemoryRegion, 1);
    int i;

    /* Initialize SoC */
    object_initialize_child(OBJECT(machine), "soc", &s->soc, TYPE_RISCV_MLTON_SOC);
    qdev_realize(DEVICE(&s->soc), NULL, &error_abort);

    /* Data Tightly Integrated Memory */
    memory_region_init_ram(main_mem, NULL, "riscv.mlton.ram",
        memmap[MLTON_DEV_RAM].size, &error_fatal);
    memory_region_add_subregion(sys_mem,
        memmap[MLTON_DEV_RAM].base, main_mem);

    /* Mask ROM reset vector */
    uint32_t reset_vec[4];

    reset_vec[0] = 0x00000000;      /* 0x1000: unimp                        */
    reset_vec[1] = 0x00800293;      /* 0x1004: li       t0,8                */
    reset_vec[2] = 0x01c29293;      /* 0x1008: slli	    t0,t0,0x1c          */
    reset_vec[3] = 0x00028067;      /* 0x100c: jr       t0                  */

    /* copy in the reset vector in little_endian byte order */
    for (i = 0; i < sizeof(reset_vec) >> 2; i++) {
        reset_vec[i] = cpu_to_le32(reset_vec[i]);
    }
    rom_add_blob_fixed_as("mrom.reset", reset_vec, sizeof(reset_vec),
                          memmap[MLTON_DEV_MROM].base, &address_space_memory);

    if (machine->kernel_filename) {
        riscv_load_kernel(machine->kernel_filename,
                          memmap[MLTON_DEV_RAM].base, NULL);
    }
}

static void mlton_machine_instance_init(Object *obj)
{
    // MLtonState *s = RISCV_MLTON_MACHINE(obj);
}

static void mlton_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "RISC-V Board for MLton testing";
    mc->init = mlton_machine_init;
    mc->max_cpus = 2;
    mc->default_cpu_type = TYPE_RISCV_CPU_ANY;
}

static const TypeInfo mlton_machine_typeinfo = {
    .name       = MACHINE_TYPE_NAME("mlton"),
    .parent     = TYPE_MACHINE,
    .class_init = mlton_machine_class_init,
    .instance_init = mlton_machine_instance_init,
    .instance_size = sizeof(MLtonState),
};

static void mlton_machine_init_register_types(void)
{
    type_register_static(&mlton_machine_typeinfo);
}

type_init(mlton_machine_init_register_types)

static void mlton_soc_init(Object *obj)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    MLtonSoCState *s = RISCV_MLTON_SOC(obj);

    object_initialize_child(obj, "cpus", &s->cpus, TYPE_RISCV_HART_ARRAY);
    object_property_set_int(OBJECT(&s->cpus), "num-harts", ms->smp.cpus,
                            &error_abort);
    object_property_set_int(OBJECT(&s->cpus), "resetvec", 0x1004, &error_abort);
}

static void mlton_soc_realize(DeviceState *dev, Error **errp)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    const MemMapEntry *memmap = mlton_memmap;
    MLtonSoCState *s = RISCV_MLTON_SOC(dev);
    MemoryRegion *sys_mem = get_system_memory();

    object_property_set_str(OBJECT(&s->cpus), "cpu-type", ms->cpu_type,
                            &error_abort);
    sysbus_realize(SYS_BUS_DEVICE(&s->cpus), &error_abort);

    /* Mask ROM */
    memory_region_init_rom(&s->mask_rom, OBJECT(dev), "riscv.mlton.mrom",
                           memmap[MLTON_DEV_MROM].size, &error_fatal);
    memory_region_add_subregion(sys_mem,
        memmap[MLTON_DEV_MROM].base, &s->mask_rom);

    serial_mm_init(sys_mem, memmap[MLTON_DEV_UART].base,
        0, NULL, 399193,
        serial_hd(0), DEVICE_LITTLE_ENDIAN);

}

static void mlton_soc_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = mlton_soc_realize;
    /* Reason: Uses serial_hds in realize function, thus can't be used twice */
    dc->user_creatable = false;
}

static const TypeInfo mlton_soc_type_info = {
    .name = TYPE_RISCV_MLTON_SOC,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(MLtonSoCState),
    .instance_init = mlton_soc_init,
    .class_init = mlton_soc_class_init,
};

static void mlton_soc_register_types(void)
{
    type_register_static(&mlton_soc_type_info);
}

type_init(mlton_soc_register_types)
