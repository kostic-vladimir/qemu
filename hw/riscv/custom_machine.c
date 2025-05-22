#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/typedefs.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "qom/object.h"
#include "hw/boards.h"
#include "hw/sysbus.h"
#include "system/address-spaces.h"
#include "target/riscv/cpu.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/boot.h"
#include "system/memory.h"

#define TYPE_CUSTOM_MACHINE MACHINE_TYPE_NAME("custom")
typedef struct CustomMachineState CustomMachineState;
DECLARE_INSTANCE_CHECKER(CustomMachineState, CUSTOM_MACHINE, TYPE_CUSTOM_MACHINE)

enum {
    CUSTOM_SRAM
};

static const MemMapEntry custom_memmap[] = {
    [CUSTOM_SRAM] =    { 0x40000000,        MiB * 2 },
};

struct CustomMachineState {
    /*< private >*/
    MachineState parent;

    /*< public >*/
    RISCVHartArrayState soc;

    char *sram_path;
};

static void custom_board_init(MachineState *machine)
{
    CustomMachineState *state = CUSTOM_MACHINE(machine);

    target_ulong firmware_end_addr = custom_memmap[CUSTOM_SRAM].base;
    hwaddr firmware_load_addr = custom_memmap[CUSTOM_SRAM].base;


    if (!state->sram_path) {
        error_report("SRAM SHMEM backend must be provided!");
        exit(1);
    }
    
    // NOTE: this is temporary, the firmware should already be in SHMEM
    if (!machine->firmware) {
        error_report("Firmware must be provided!");
        exit(1);
    }

    info_report("Initializing SoC...");
    object_initialize_child(OBJECT(machine), "soc", &state->soc, TYPE_RISCV_HART_ARRAY);

    object_property_set_str(OBJECT(&state->soc), "cpu-type", machine->cpu_type, &error_abort);
    object_property_set_int(OBJECT(&state->soc), "hartid-base", 0, &error_abort);
    object_property_set_int(OBJECT(&state->soc), "num-harts", 1, &error_abort);
    object_property_set_uint(OBJECT(&state->soc), "resetvec", firmware_load_addr, &error_abort);
    sysbus_realize(SYS_BUS_DEVICE(&state->soc), &error_abort);

    info_report("\tCPU type:           %s", state->soc.cpu_type);
    info_report("\tHart num:           %d", state->soc.num_harts);
    info_report("\tBase Hart ID:       %d", state->soc.hartid_base);
    info_report("\tReset vector:       0x%lx", state->soc.resetvec);
    info_report(" ");

    MemoryRegion *sram = g_new(MemoryRegion, 1);
    bool err = memory_region_init_ram_from_file(sram, NULL, "custom_machine.ram", 
                                                custom_memmap[CUSTOM_SRAM].size, 0, RAM_SHARED, 
                                                state->sram_path, 0, &error_fatal);
    if (!err) {
        error_reportf_err(error_fatal, "Failed to initialize SRAM from file %s, exit with error: ", state->sram_path);
        exit(1);
    }
    info_report("SRAM Linux SHMEM backend: %s", state->sram_path);
    info_report(" ");

    memory_region_add_subregion(get_system_memory(), custom_memmap[CUSTOM_SRAM].base, sram);


    info_report("Loading firmware...");
    firmware_end_addr = riscv_load_firmware(machine->firmware,
                                            &firmware_load_addr,
                                            NULL);

    info_report("\tFirmware file name: %s", machine->firmware);
    info_report("\tFirmware load addr: 0x%lx", firmware_load_addr);
    info_report("\tFirmware size:      %ld bytes", firmware_end_addr - firmware_load_addr);
    info_report(" ");
                                       
    info_report("Running...");
}

static void set_sram_path(Object *obj, const char *val, Error **errp)
{
    CustomMachineState *state = CUSTOM_MACHINE(obj);
    state->sram_path = g_strdup(val);
}

static void custom_machine_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "RISC-V Custom board";
    mc->init = custom_board_init;
    mc->max_cpus = 1;
    mc->min_cpus = 1;
    mc->default_cpus = mc->min_cpus;
    mc->default_cpu_type = TYPE_RISCV_CPU_BASE;

    object_class_property_add_str(oc, "sram-shmem", NULL, set_sram_path);
    object_class_property_set_description(oc, "sram-shmem",
                                          "Filepath to SRAM Linux SHMEM");
}

static void custom_machine_instance_init(Object *obj)
{
}

static const TypeInfo custom_machine_typeinfo = {
    .name       = TYPE_CUSTOM_MACHINE,
    .parent     = TYPE_MACHINE,
    .class_init = custom_machine_class_init,
    .instance_init = custom_machine_instance_init,
    .instance_size = sizeof(CustomMachineState),
};

static void custom_machine_init_register_types(void)
{
    type_register_static(&custom_machine_typeinfo);
}

type_init(custom_machine_init_register_types)