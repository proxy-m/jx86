#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <stdbool.h>

#include "const.h"
#include "global_pointers.h"

// XXX: Remove these declarations when they are implemented in C
static void cmovcc16(bool);
static void cmovcc32(bool);
void jmpcc16(bool);
void jmpcc32(bool);
void setcc(bool);
void cpuid();

void bt_mem(int32_t, int32_t);
void bt_reg(int32_t, int32_t);
void bts_mem(int32_t, int32_t);
int32_t bts_reg(int32_t, int32_t);
void btc_mem(int32_t, int32_t);
int32_t btc_reg(int32_t, int32_t);
void btr_mem(int32_t, int32_t);
int32_t btr_reg(int32_t, int32_t);
int32_t bsf16(int32_t, int32_t);
int32_t bsf32(int32_t, int32_t);
int32_t bsr16(int32_t, int32_t);
int32_t bsr32(int32_t, int32_t);

int32_t popcnt(int32_t);
int32_t bswap(int32_t);

int32_t read_g16s(void);
int32_t read_reg_e16(void);
int32_t read_reg_e32s(void);

void cpl_changed(void);
void update_cs_size(int32_t);
void unimplemented_sse(void);

int32_t shld16(int32_t, int32_t, int32_t);
int32_t shld32(int32_t, int32_t, int32_t);
int32_t shrd16(int32_t, int32_t, int32_t);
int32_t shrd32(int32_t, int32_t, int32_t);

bool has_rand_int(void);
int32_t get_rand_int(void);

void todo();
void undefined_instruction();

void clear_tlb();
void full_clear_tlb();

int32_t microtick();

int32_t lsl(int32_t, int32_t);
int32_t lar(int32_t, int32_t);
int32_t verw(int32_t);
int32_t verr(int32_t);

void invlpg(int32_t);
void load_tr(int32_t);
void load_ldt(int32_t);

int32_t set_cr0(int32_t);
void writable_or_pagefault(int32_t, int32_t);

bool* const apic_enabled;


static void instr_0F00() { read_modrm_byte();
    if(!protected_mode[0] || vm86_mode())
    {
        // No GP, UD is correct here
        dbg_log("0f 00 #ud");
        trigger_ud();
    }

    switch(modrm_byte[0] >> 3 & 7)
    {
        case 0:
            // sldt
            set_e16(sreg[LDTR]);
            if(is_osize_32() && modrm_byte[0] >= 0xC0)
            {
                reg32s[modrm_byte[0] & 7] &= 0xFFFF;
            }
            break;
        case 1:
            // str
            set_e16(sreg[LDTR]);
            if(is_osize_32() && modrm_byte[0] >= 0xC0)
            {
                reg32s[modrm_byte[0] & 7] &= 0xFFFF;
            }
            break;
        case 2:
            // lldt
            if(cpl[0])
            {
                trigger_gp(0);
            }

            {
                int32_t data = read_e16();
                load_ldt(data);
            }
            break;
        case 3:
            // ltr
            if(cpl[0])
            {
                trigger_gp(0);
            }

            int32_t data = read_e16();
            load_tr(data);
            break;
        case 4:
            verr(read_e16());
            break;
        case 5:
            verw(read_e16());
            break;

        default:
            dbg_log("%d", modrm_byte[0] >> 3 & 7);
            todo();
    }
}

static void instr_0F01() { read_modrm_byte();
    int32_t mod = modrm_byte[0] >> 3 & 7;

    if(mod == 4)
    {
        // smsw
        if(modrm_byte[0] >= 0xC0 && is_osize_32())
        {
            set_e32(cr[0]);
        }
        else
        {
            set_e16(cr[0]);
        }
        return;
    }
    else if(mod == 6)
    {
        // lmsw
        if(cpl[0])
        {
            trigger_gp(0);
        }

        int32_t cr0 = read_e16();

        cr0 = (cr[0] & ~0xF) | (cr0 & 0xF);

        if(protected_mode[0])
        {
            // lmsw cannot be used to switch back
            cr0 |= CR0_PE;
        }

        set_cr0(cr0);
        return;
    }

    if(modrm_byte[0] >= 0xC0)
    {
        // only memory
        dbg_log("0f 01 #ud");
        trigger_ud();
    }

    int32_t addr = modrm_resolve(modrm_byte[0]);

    switch(mod)
    {
        case 0:
            // sgdt
            writable_or_pagefault(addr, 6);
            safe_write16(addr, gdtr_size[0]);
            {
                int32_t mask = is_osize_32() ? -1 : 0x00FFFFFF;
                safe_write32(addr + 2, gdtr_offset[0] & mask);
            }
            break;
        case 1:
            // sidt
            writable_or_pagefault(addr, 6);
            safe_write16(addr, idtr_size[0]);
            {
                int32_t mask = is_osize_32() ? -1 : 0x00FFFFFF;
                safe_write32(addr + 2, idtr_offset[0] & mask);
            }
            break;
        case 2:
            // lgdt
            if(cpl[0])
            {
                trigger_gp(0);
            }

            {
                int32_t size = safe_read16(addr);
                int32_t offset = safe_read32s(addr + 2);

                gdtr_size[0] = size;
                gdtr_offset[0] = offset;

                if(!is_osize_32())
                {
                    gdtr_offset[0] &= 0xFFFFFF;
                }

                //dbg_log("gdt at " + h(gdtr_offset[0]) + ", " + gdtr_size[0] + " bytes");
                //debug.dump_state();
                //debug.dump_regs_short();
                //debug.dump_gdt_ldt();
            }
            break;
        case 3:
            // lidt
            if(cpl[0])
            {
                trigger_gp(0);
            }

            {
                int32_t size = safe_read16(addr);
                int32_t offset = safe_read32s(addr + 2);

                idtr_size[0] = size;
                idtr_offset[0] = offset;

                if(!is_osize_32())
                {
                    idtr_offset[0] &= 0xFFFFFF;
                }

                //dbg_log("[" + h(instruction_pointer) + "] idt at " +
                //        h(idtr_offset) + ", " + idtr_size[0] + " bytes " + h(addr));
            }
            break;
        case 7:
            // flush translation lookaside buffer
            if(cpl[0])
            {
                trigger_gp(0);
            }

            invlpg(addr);
            break;
        default:
            dbg_log("%d", mod);
            todo();
    }
}

static void instr16_0F02() { read_modrm_byte();
    // lar
    if(!protected_mode[0] || vm86_mode())
    {
        dbg_log("lar #ud");
        trigger_ud();
    }
    int32_t data = read_e16();
    write_g16(lar(data, read_g16()));
}
static void instr32_0F02() { read_modrm_byte();
    if(!protected_mode[0] || vm86_mode())
    {
        dbg_log("lar #ud");
        trigger_ud();
    }
    int32_t data = read_e16();
    write_g32(lar(data, read_g32s()));
}

static void instr16_0F03() { read_modrm_byte();
    // lsl
    if(!protected_mode[0] || vm86_mode())
    {
        dbg_log("lsl #ud");
        trigger_ud();
    }
    int32_t data = read_e16();
    write_g16(lsl(data, read_g16()));
}
static void instr32_0F03() { read_modrm_byte();
    if(!protected_mode[0] || vm86_mode())
    {
        dbg_log("lsl #ud");
        trigger_ud();
    }
    int32_t data = read_e16();
    write_g32(lsl(data, read_g32s()));
}

static void instr_0F04() { undefined_instruction(); }
static void instr_0F05() { undefined_instruction(); }

static void instr_0F06() {
    // clts
    if(cpl[0])
    {
        dbg_log("clts #gp");
        trigger_gp(0);
    }
    else
    {
        //dbg_log("clts");
        cr[0] &= ~CR0_TS;
    }
}

static void instr_0F07() { undefined_instruction(); }
static void instr_0F08() {
    // invd
    todo();
}

static void instr_0F09() {
    if(cpl[0])
    {
        dbg_log("wbinvd #gp");
        trigger_gp(0);
    }
    // wbinvd
}


static void instr_0F0A() { undefined_instruction(); }
static void instr_0F0B() {
    // UD2
    trigger_ud();
}
static void instr_0F0C() { undefined_instruction(); }

static void instr_0F0D() {
    // nop
    todo();
}

static void instr_0F0E() { undefined_instruction(); }
static void instr_0F0F() { undefined_instruction(); }

static void instr_0F10() { unimplemented_sse(); }
static void instr_0F11() { unimplemented_sse(); }
static void instr_0F12() { unimplemented_sse(); }
static void instr_660F12() { unimplemented_sse(); }
static void instr_0F13() { unimplemented_sse(); }
static void instr_660F13() { unimplemented_sse(); }
static void instr_0F14() { unimplemented_sse(); }
static void instr_660F14() { unimplemented_sse(); }
static void instr_0F15() { unimplemented_sse(); }
static void instr_0F16() { unimplemented_sse(); }
static void instr_0F17() { unimplemented_sse(); }

static void instr_0F18() { read_modrm_byte();
    // prefetch
    // nop for us
    if(modrm_byte[0] < 0xC0)
        modrm_resolve(modrm_byte[0]);
}

static void instr_0F19() { unimplemented_sse(); }
static void instr_0F1A() { unimplemented_sse(); }
static void instr_0F1B() { unimplemented_sse(); }
static void instr_0F1C() { unimplemented_sse(); }
static void instr_0F1D() { unimplemented_sse(); }
static void instr_0F1E() { unimplemented_sse(); }
static void instr_0F1F() { read_modrm_byte();
    // multi-byte nop
    if(modrm_byte[0] < 0xC0)
        modrm_resolve(modrm_byte[0]);
}


static void instr_0F20() { read_modrm_byte();

    if(cpl[0])
    {
        trigger_gp(0);
    }
    //dbg_log("cr" + (modrm_byte[0] >> 3 & 7) + " read");

    // mov addr, cr
    // mod = which control register
    switch(modrm_byte[0] >> 3 & 7)
    {
        case 0:
            write_reg_e32(cr[0]);
            break;
        case 2:
            //dbg_log("read cr2 at " + h(instruction_pointer, 8));
            write_reg_e32(cr[2]);
            break;
        case 3:
            //dbg_log("read cr3 (" + h(cr[3], 8) + ")");
            write_reg_e32(cr[3]);
            break;
        case 4:
            write_reg_e32(cr[4]);
            break;
        default:
            dbg_log("%d", modrm_byte[0] >> 3 & 7);
            todo();
    }
}

static void instr_0F21() { read_modrm_byte();
    if(cpl[0])
    {
        trigger_gp(0);
    }

    int32_t dreg_index = modrm_byte[0] >> 3 & 7;
    if((cr[4] & CR4_DE) && (dreg_index == 4 || dreg_index == 5))
    {
        dbg_log("#ud mov dreg 4/5 with cr4.DE set");
        trigger_ud();
    }

    // high two bits of modrm are ignored
    reg32s[modrm_byte[0] & 7] = dreg[dreg_index];

    //dbg_log("read dr" + dreg + ": " + h(dreg[dreg_index]));
}

static void instr_0F22() { read_modrm_byte();

    if(cpl[0])
    {
        trigger_gp(0);
    }

    int32_t data = read_reg_e32s();
    //dbg_log("cr" + (modrm_byte[0] >> 3 & 7) + " written: " + h(data, 8));

    // mov cr, addr
    // mod = which control register
    switch(modrm_byte[0] >> 3 & 7)
    {
        case 0:
            set_cr0(data);
            //dbg_log("cr0=" + h(data));
            break;

        case 2:
            cr[2] = data;
            //dbg_log("cr2=" + h(data));
            break;

        case 3:
            //dbg_log("cr3=" + h(data));
            data &= ~0b111111100111;
            dbg_assert_message((data & 0xFFF) == 0, "TODO");
            cr[3] = data;
            clear_tlb();

            //dump_page_directory();
            //dbg_log("page directory loaded at " + h(cr[3], 8));
            break;

        case 4:
            if(data & (1 << 11 | 1 << 12 | 1 << 15 | 1 << 16 | 1 << 19 | 0xFFC00000))
            {
                trigger_gp(0);
            }

            if((cr[4] ^ data) & CR4_PGE)
            {
                if(data & CR4_PGE)
                {
                    // The PGE bit has been enabled. The global TLB is
                    // still empty, so we only have to copy it over
                    clear_tlb();
                }
                else
                {
                    // Clear the global TLB
                    full_clear_tlb();
                }
            }

            cr[4] = data;
            page_size_extensions[0] = (cr[4] & CR4_PSE) ? PSE_ENABLED : 0;

            if(cr[4] & CR4_PAE)
            {
                //throw debug.unimpl("PAE");
                assert(false);
            }

            //dbg_log("cr4=%d", cr[4]);
            break;

        default:
            dbg_log("%d", modrm_byte[0] >> 3 & 7);
            todo();
    }
}
static void instr_0F23() { read_modrm_byte();
    if(cpl[0])
    {
        trigger_gp(0);
    }

    int32_t dreg_index = modrm_byte[0] >> 3 & 7;
    if((cr[4] & CR4_DE) && (dreg_index == 4 || dreg_index == 5))
    {
        dbg_log("#ud mov dreg 4/5 with cr4.DE set");
        trigger_ud();
    }

    // high two bits of modrm are ignored
    dreg[dreg_index] = read_reg_e32s();

    //dbg_log("write dr" + dreg + ": " + h(dreg[dreg_index]));
}

static void instr_0F24() { undefined_instruction(); }
static void instr_0F25() { undefined_instruction(); }
static void instr_0F26() { undefined_instruction(); }
static void instr_0F27() { undefined_instruction(); }

static void instr_0F28() { unimplemented_sse(); }
static void instr_660F28() { unimplemented_sse(); }
static void instr_0F29() {
    // movaps xmm/m128, xmm
    task_switch_test_mmx();
    read_modrm_byte();

    union reg128 data = read_xmm128s();
    assert(*modrm_byte < 0xC0);
    int32_t addr = modrm_resolve(*modrm_byte);
    safe_write128(addr, data);
}
static void instr_660F29() { unimplemented_sse(); }
static void instr_0F2A() { unimplemented_sse(); }
static void instr_0F2B() { unimplemented_sse(); }
static void instr_660F2B() { unimplemented_sse(); }
static void instr_0F2C() { unimplemented_sse(); }
static void instr_F20F2C() { unimplemented_sse(); }
static void instr_0F2D() { unimplemented_sse(); }
static void instr_0F2E() { unimplemented_sse(); }
static void instr_0F2F() { unimplemented_sse(); }

// wrmsr
static void instr_0F30() {
    // wrmsr - write maschine specific register

    if(cpl[0])
    {
        // cpl > 0 or vm86 mode (vm86 mode is always runs with cpl=3)
        trigger_gp(0);
    }

    int32_t index = reg32s[ECX];
    int32_t low = reg32s[EAX];
    int32_t high = reg32s[EDX];

    if(index != IA32_SYSENTER_ESP)
    {
        //dbg_log("wrmsr ecx=" + h(index, 8) +
        //            " data=" + h(high, 8) + ":" + h(low, 8));
    }

    switch(index)
    {
        case IA32_SYSENTER_CS:
            sysenter_cs[0] = low & 0xFFFF;
            break;

        case IA32_SYSENTER_EIP:
            sysenter_eip[0] = low;
            break;

        case IA32_SYSENTER_ESP:
            sysenter_esp[0] = low;
            break;

        case IA32_APIC_BASE_MSR:
            {
                dbg_assert_message(high == 0, "Changing APIC address (high 32 bits) not supported");
                int32_t address = low & ~(IA32_APIC_BASE_BSP | IA32_APIC_BASE_EXTD | IA32_APIC_BASE_EN);
                dbg_assert_message(address == APIC_ADDRESS, "Changing APIC address not supported");
                dbg_assert_message((low & IA32_APIC_BASE_EXTD) == 0, "x2apic not supported");
                *apic_enabled = (low & IA32_APIC_BASE_EN) == IA32_APIC_BASE_EN;
            }
            break;

        case IA32_TIME_STAMP_COUNTER:
            {
                int32_t new_tick = (low) + 0x100000000 * (high);
                tsc_offset[0] = microtick() - new_tick / TSC_RATE; // XXX: float
            }
            break;

        case IA32_BIOS_SIGN_ID:
            break;

        case IA32_MISC_ENABLE: // Enable Misc. Processor Features
            break;

        case IA32_MCG_CAP:
            // netbsd
            break;

        case IA32_KERNEL_GS_BASE:
            // Only used in 64 bit mode (by SWAPGS), but set by kvm-unit-test
            dbg_log("GS Base written");
            break;

        default:
            assert(false);
            //dbg_assert(false, "Unknown msr: " + h(index, 8));
    }
}

static void instr_0F31() {
    // rdtsc - read timestamp counter

    if(!cpl[0] || !(cr[4] & CR4_TSD))
    {
        int32_t n = microtick() - tsc_offset[0]; // XXX: float
        //dbg_assert(isFinite(n), "non-finite tsc: " + n);

        reg32s[EAX] = n * TSC_RATE;
        reg32s[EDX] = n * (TSC_RATE / 0x100000000);

        //dbg_log("rdtsc  edx:eax=" + h(reg32[EDX], 8) + ":" + h(reg32[EAX], 8));
    }
    else
    {
        trigger_gp(0);
    }
}

static void instr_0F32() {
    // rdmsr - read maschine specific register
    if(cpl[0])
    {
        trigger_gp(0);
    }

    int32_t index = reg32s[ECX];

    //dbg_log("rdmsr ecx=" + h(index, 8));

    int32_t low = 0;
    int32_t high = 0;

    switch(index)
    {
        case IA32_SYSENTER_CS:
            low = sysenter_cs[0];
            break;

        case IA32_SYSENTER_EIP:
            low = sysenter_eip[0];
            break;

        case IA32_SYSENTER_ESP:
            low = sysenter_esp[0];
            break;

        case IA32_TIME_STAMP_COUNTER:
            {
                int32_t n = microtick() - tsc_offset[0]; // XXX: float
                low = n * TSC_RATE;
                high = n * (TSC_RATE / 0x100000000);
            }
            break;

        case IA32_PLATFORM_ID:
            break;

        case IA32_APIC_BASE_MSR:
            if(ENABLE_ACPI)
            {
                low = APIC_ADDRESS;

                if(*apic_enabled)
                {
                    low |= IA32_APIC_BASE_EN;
                }
            }
            break;

        case IA32_BIOS_SIGN_ID:
            break;

        case IA32_MISC_ENABLE: // Enable Misc. Processor Features
            break;

        case IA32_RTIT_CTL:
            // linux4
            break;

        case MSR_SMI_COUNT:
            break;

        case IA32_MCG_CAP:
            // netbsd
            break;

        case MSR_PKG_C2_RESIDENCY:
            break;

        default:
            assert(false);
            //dbg_assert(false, "Unknown msr: " + h(index, 8));
    }

    reg32s[EAX] = low;
    reg32s[EDX] = high;
}

static void instr_0F33() {
    // rdpmc
    todo();
}

static void instr_0F34() {
    // sysenter
    int32_t seg = sysenter_cs[0] & 0xFFFC;

    if(!protected_mode[0] || seg == 0)
    {
        trigger_gp(0);
    }

    if(CPU_LOG_VERBOSE)
    {
        //dbg_log("sysenter  cs:eip=" + h(seg    , 4) + ":" + h(sysenter_eip[0], 8) +
        //                 " ss:esp=" + h(seg + 8, 4) + ":" + h(sysenter_esp[0], 8));
    }

    flags[0] &= ~FLAG_VM & ~FLAG_INTERRUPT;

    instruction_pointer[0] = sysenter_eip[0];
    reg32s[ESP] = sysenter_esp[0];

    sreg[CS] = seg;
    segment_is_null[CS] = 0;
    segment_limits[CS] = -1;
    segment_offsets[CS] = 0;

    update_cs_size(true);

    cpl[0] = 0;
    cpl_changed();

    sreg[SS] = seg + 8;
    segment_is_null[SS] = 0;
    segment_limits[SS] = -1;
    segment_offsets[SS] = 0;

    stack_size_32[0] = true;
    diverged();
}

static void instr_0F35() {
    // sysexit
    int32_t seg = sysenter_cs[0] & 0xFFFC;

    if(!protected_mode[0] || cpl[0] || seg == 0)
    {
        trigger_gp(0);
    }

    if(CPU_LOG_VERBOSE)
    {
        //dbg_log("sysexit  cs:eip=" + h(seg + 16, 4) + ":" + h(reg32s[EDX], 8) +
        //                 " ss:esp=" + h(seg + 24, 4) + ":" + h(reg32s[ECX], 8));
    }

    instruction_pointer[0] = reg32s[EDX];
    reg32s[ESP] = reg32s[ECX];

    sreg[CS] = seg + 16 | 3;

    segment_is_null[CS] = 0;
    segment_limits[CS] = -1;
    segment_offsets[CS] = 0;

    update_cs_size(true);

    cpl[0] = 3;
    cpl_changed();

    sreg[SS] = seg + 24 | 3;
    segment_is_null[SS] = 0;
    segment_limits[SS] = -1;
    segment_offsets[SS] = 0;

    stack_size_32[0] = true;
    diverged();
}

static void instr_0F36() { undefined_instruction(); }

static void instr_0F37() {
    // getsec
    todo();
}

static void instr_0F38() { unimplemented_sse(); }
static void instr_0F39() { unimplemented_sse(); }
static void instr_0F3A() { unimplemented_sse(); }
static void instr_0F3B() { unimplemented_sse(); }
static void instr_0F3C() { unimplemented_sse(); }
static void instr_0F3D() { unimplemented_sse(); }
static void instr_0F3E() { unimplemented_sse(); }
static void instr_0F3F() { unimplemented_sse(); }


// cmov
static void instr16_0F40() { read_modrm_byte(); cmovcc16( test_o()); }
static void instr32_0F40() { read_modrm_byte(); cmovcc32( test_o()); }
static void instr16_0F41() { read_modrm_byte(); cmovcc16(!test_o()); }
static void instr32_0F41() { read_modrm_byte(); cmovcc32(!test_o()); }
static void instr16_0F42() { read_modrm_byte(); cmovcc16( test_b()); }
static void instr32_0F42() { read_modrm_byte(); cmovcc32( test_b()); }
static void instr16_0F43() { read_modrm_byte(); cmovcc16(!test_b()); }
static void instr32_0F43() { read_modrm_byte(); cmovcc32(!test_b()); }
static void instr16_0F44() { read_modrm_byte(); cmovcc16( test_z()); }
static void instr32_0F44() { read_modrm_byte(); cmovcc32( test_z()); }
static void instr16_0F45() { read_modrm_byte(); cmovcc16(!test_z()); }
static void instr32_0F45() { read_modrm_byte(); cmovcc32(!test_z()); }
static void instr16_0F46() { read_modrm_byte(); cmovcc16( test_be()); }
static void instr32_0F46() { read_modrm_byte(); cmovcc32( test_be()); }
static void instr16_0F47() { read_modrm_byte(); cmovcc16(!test_be()); }
static void instr32_0F47() { read_modrm_byte(); cmovcc32(!test_be()); }
static void instr16_0F48() { read_modrm_byte(); cmovcc16( test_s()); }
static void instr32_0F48() { read_modrm_byte(); cmovcc32( test_s()); }
static void instr16_0F49() { read_modrm_byte(); cmovcc16(!test_s()); }
static void instr32_0F49() { read_modrm_byte(); cmovcc32(!test_s()); }
static void instr16_0F4A() { read_modrm_byte(); cmovcc16( test_p()); }
static void instr32_0F4A() { read_modrm_byte(); cmovcc32( test_p()); }
static void instr16_0F4B() { read_modrm_byte(); cmovcc16(!test_p()); }
static void instr32_0F4B() { read_modrm_byte(); cmovcc32(!test_p()); }
static void instr16_0F4C() { read_modrm_byte(); cmovcc16( test_l()); }
static void instr32_0F4C() { read_modrm_byte(); cmovcc32( test_l()); }
static void instr16_0F4D() { read_modrm_byte(); cmovcc16(!test_l()); }
static void instr32_0F4D() { read_modrm_byte(); cmovcc32(!test_l()); }
static void instr16_0F4E() { read_modrm_byte(); cmovcc16( test_le()); }
static void instr32_0F4E() { read_modrm_byte(); cmovcc32( test_le()); }
static void instr16_0F4F() { read_modrm_byte(); cmovcc16(!test_le()); }
static void instr32_0F4F() { read_modrm_byte(); cmovcc32(!test_le()); }


static void instr_0F50() { unimplemented_sse(); }
static void instr_0F51() { unimplemented_sse(); }
static void instr_0F52() { unimplemented_sse(); }
static void instr_0F53() { unimplemented_sse(); }
static void instr_0F54() { unimplemented_sse(); }
static void instr_660F54() { unimplemented_sse(); }
static void instr_0F55() { unimplemented_sse(); }
static void instr_0F56() { unimplemented_sse(); }
static void instr_0F57() { unimplemented_sse(); }
static void instr_660F57() { unimplemented_sse(); }

static void instr_0F58() { unimplemented_sse(); }
static void instr_0F59() { unimplemented_sse(); }
static void instr_0F5A() { unimplemented_sse(); }
static void instr_0F5B() { unimplemented_sse(); }
static void instr_0F5C() { unimplemented_sse(); }
static void instr_0F5D() { unimplemented_sse(); }
static void instr_0F5E() { unimplemented_sse(); }
static void instr_0F5F() { unimplemented_sse(); }

static void instr_0F60() { unimplemented_sse(); }
void instr_660F60() {
    // punpcklbw xmm, xmm/m128
    task_switch_test_mmx();
    read_modrm_byte();
    union reg64 source = read_xmm_mem64s();
    union reg64 destination = read_xmm64s();
    write_xmm128s(
        destination.u8[0] | source.u8[0] << 8 | destination.u8[1] << 16 | source.u8[1] << 24,
        destination.u8[2] | source.u8[2] << 8 | destination.u8[3] << 16 | source.u8[3] << 24,
        destination.u8[4] | source.u8[4] << 8 | destination.u8[5] << 16 | source.u8[5] << 24,
        destination.u8[6] | source.u8[6] << 8 | destination.u8[7] << 16 | source.u8[7] << 24
    );
}
static void instr_0F61() { unimplemented_sse(); }
static void instr_660F61() { unimplemented_sse(); }
static void instr_0F62() { unimplemented_sse(); }
static void instr_0F63() { unimplemented_sse(); }
static void instr_0F64() { unimplemented_sse(); }
static void instr_0F65() { unimplemented_sse(); }
static void instr_0F66() { unimplemented_sse(); }
static void instr_0F67() { unimplemented_sse(); }
static void instr_660F67() { unimplemented_sse(); }

static void instr_0F68() { unimplemented_sse(); }
static void instr_660F68() { unimplemented_sse(); }
static void instr_0F69() { unimplemented_sse(); }
static void instr_0F6A() { unimplemented_sse(); }
static void instr_0F6B() { unimplemented_sse(); }
static void instr_0F6C() { unimplemented_sse(); }
static void instr_0F6D() { unimplemented_sse(); }
static void instr_0F6E() { unimplemented_sse(); }
static void instr_660F6E() {
    // movd mm, r/m32
    task_switch_test_mmx();
    read_modrm_byte();
    int32_t data = read_e32s();
    write_xmm128s(data, 0, 0, 0);
}
static void instr_0F6F() { unimplemented_sse(); }
static void instr_660F6F() {
    // movdqa xmm, xmm/mem128
    task_switch_test_mmx();
    read_modrm_byte();
    union reg128 data = read_xmm_mem128s();
    write_xmm128s(data.u32[0], data.u32[1], data.u32[2], data.u32[3]);
}
static void instr_F30F6F() {
    // movdqu xmm, xmm/m128
    task_switch_test_mmx();
    read_modrm_byte();
    union reg128 data = read_xmm_mem128s();
    write_xmm128s(data.u32[0], data.u32[1], data.u32[2], data.u32[3]);
}

static void instr_0F70() { unimplemented_sse(); }
static void instr_660F70() {
    // pshufd xmm, xmm/mem128
    task_switch_test_mmx();
    read_modrm_byte();
    union reg128 source = read_xmm_mem128s();
    int32_t order = read_op8();

    write_xmm128s(
        source.u32[order & 3],
        source.u32[order >> 2 & 3],
        source.u32[order >> 4 & 3],
        source.u32[order >> 6 & 3]
    );
}
static void instr_F20F70() { unimplemented_sse(); }
static void instr_F30F70() { unimplemented_sse(); }
static void instr_0F71() { unimplemented_sse(); }
static void instr_0F72() { unimplemented_sse(); }
static void instr_0F73() { unimplemented_sse(); }
static void instr_660F73() { unimplemented_sse(); }
static void instr_0F74() { unimplemented_sse(); }
static void instr_660F74() {
    // pcmpeqb xmm, xmm/m128
    task_switch_test_mmx();
    read_modrm_byte();

    union reg128 source = read_xmm_mem128s();
    union reg128 destination = read_xmm128s();

    union reg128 result;

    for(int32_t i = 0; i < 16; i++)
    {
        result.u8[i] = source.u8[i] == destination.u8[i] ? 0xFF : 0;
    }

    write_xmm128s(result.u32[0], result.u32[1], result.u32[2], result.u32[3]);
}
static void instr_0F75() { unimplemented_sse(); }
static void instr_660F75() { unimplemented_sse(); }
static void instr_0F76() { unimplemented_sse(); }
static void instr_660F76() { unimplemented_sse(); }
static void instr_0F77() {
    // emms
    dbg_assert((*prefixes & (PREFIX_MASK_REP | PREFIX_MASK_OPSIZE)) == 0);

    if(cr[0] & (CR0_EM | CR0_TS)) {
        if(cr[0] & CR0_TS) {
            trigger_nm();
        }
        else {
            trigger_ud();
        }
    }

    safe_tag_word(0xFFFF);
}

static void instr_0F78() { unimplemented_sse(); }
static void instr_0F79() { unimplemented_sse(); }
static void instr_0F7A() { unimplemented_sse(); }
static void instr_0F7B() { unimplemented_sse(); }
static void instr_0F7C() { unimplemented_sse(); }
static void instr_0F7D() { unimplemented_sse(); }
static void instr_0F7E() { unimplemented_sse(); }
static void instr_660F7E() {
    // movd r/m32, xmm
    task_switch_test_mmx();
    read_modrm_byte();
    union reg64 data = read_xmm64s();
    set_e32(data.u32[0]);
}
static void instr_F30F7E() {
    // movq xmm, xmm/mem64
    task_switch_test_mmx();
    read_modrm_byte();
    union reg64 data = read_xmm_mem64s();
    write_xmm128s(data.u32[0], data.u32[1], 0, 0);
}
static void instr_0F7F() { unimplemented_sse(); }
static void instr_660F7F() {
    // movdqa xmm/m128, xmm
    task_switch_test_mmx();
    read_modrm_byte();
    union reg128 data = read_xmm128s();
    assert(*modrm_byte < 0xC0);
    int32_t addr = modrm_resolve(*modrm_byte);
    safe_write128(addr, data);
}
static void instr_F30F7F() {
    // movdqu xmm/m128, xmm
    task_switch_test_mmx();
    read_modrm_byte();
    union reg128 data = read_xmm128s();
    assert(*modrm_byte < 0xC0);
    int32_t addr = modrm_resolve(*modrm_byte);
    safe_write128(addr, data);
}

// jmpcc
static void instr16_0F80() { jmpcc16( test_o()); }
static void instr32_0F80() { jmpcc32( test_o()); }
static void instr16_0F81() { jmpcc16(!test_o()); }
static void instr32_0F81() { jmpcc32(!test_o()); }
static void instr16_0F82() { jmpcc16( test_b()); }
static void instr32_0F82() { jmpcc32( test_b()); }
static void instr16_0F83() { jmpcc16(!test_b()); }
static void instr32_0F83() { jmpcc32(!test_b()); }
static void instr16_0F84() { jmpcc16( test_z()); }
static void instr32_0F84() { jmpcc32( test_z()); }
static void instr16_0F85() { jmpcc16(!test_z()); }
static void instr32_0F85() { jmpcc32(!test_z()); }
static void instr16_0F86() { jmpcc16( test_be()); }
static void instr32_0F86() { jmpcc32( test_be()); }
static void instr16_0F87() { jmpcc16(!test_be()); }
static void instr32_0F87() { jmpcc32(!test_be()); }
static void instr16_0F88() { jmpcc16( test_s()); }
static void instr32_0F88() { jmpcc32( test_s()); }
static void instr16_0F89() { jmpcc16(!test_s()); }
static void instr32_0F89() { jmpcc32(!test_s()); }
static void instr16_0F8A() { jmpcc16( test_p()); }
static void instr32_0F8A() { jmpcc32( test_p()); }
static void instr16_0F8B() { jmpcc16(!test_p()); }
static void instr32_0F8B() { jmpcc32(!test_p()); }
static void instr16_0F8C() { jmpcc16( test_l()); }
static void instr32_0F8C() { jmpcc32( test_l()); }
static void instr16_0F8D() { jmpcc16(!test_l()); }
static void instr32_0F8D() { jmpcc32(!test_l()); }
static void instr16_0F8E() { jmpcc16( test_le()); }
static void instr32_0F8E() { jmpcc32( test_le()); }
static void instr16_0F8F() { jmpcc16(!test_le()); }
static void instr32_0F8F() { jmpcc32(!test_le()); }

// setcc
static void instr_0F90() { read_modrm_byte(); setcc( test_o()); }
static void instr_0F91() { read_modrm_byte(); setcc(!test_o()); }
static void instr_0F92() { read_modrm_byte(); setcc( test_b()); }
static void instr_0F93() { read_modrm_byte(); setcc(!test_b()); }
static void instr_0F94() { read_modrm_byte(); setcc( test_z()); }
static void instr_0F95() { read_modrm_byte(); setcc(!test_z()); }
static void instr_0F96() { read_modrm_byte(); setcc( test_be()); }
static void instr_0F97() { read_modrm_byte(); setcc(!test_be()); }
static void instr_0F98() { read_modrm_byte(); setcc( test_s()); }
static void instr_0F99() { read_modrm_byte(); setcc(!test_s()); }
static void instr_0F9A() { read_modrm_byte(); setcc( test_p()); }
static void instr_0F9B() { read_modrm_byte(); setcc(!test_p()); }
static void instr_0F9C() { read_modrm_byte(); setcc( test_l()); }
static void instr_0F9D() { read_modrm_byte(); setcc(!test_l()); }
static void instr_0F9E() { read_modrm_byte(); setcc( test_le()); }
static void instr_0F9F() { read_modrm_byte(); setcc(!test_le()); }

static void instr16_0FA0() { push16(sreg[FS]); }
static void instr32_0FA0() { push32(sreg[FS]); }
static void instr16_0FA1() {
    switch_seg(FS, safe_read16(get_stack_pointer(0)));
    adjust_stack_reg(2);
}
static void instr32_0FA1() {
    switch_seg(FS, safe_read32s(get_stack_pointer(0)) & 0xFFFF);
    adjust_stack_reg(4);
}

static void instr_0FA2() { cpuid(); }

static void instr16_0FA3() { read_modrm_byte();
    if(modrm_byte[0] < 0xC0)
    {
        bt_mem(modrm_resolve(modrm_byte[0]), read_g16s());
    }
    else
    {
        bt_reg(read_reg_e16(), read_g16() & 15);
    }
}
static void instr32_0FA3() { read_modrm_byte();
    if(modrm_byte[0] < 0xC0)
    {
        bt_mem(modrm_resolve(modrm_byte[0]), read_g32s());
    }
    else
    {
        bt_reg(read_reg_e32s(), read_g32s() & 31);
    }
}

static void instr16_0FA4() { read_modrm_byte();
    int32_t data = read_write_e16(); write_e16(shld16(data, read_g16(), read_op8() & 31));
}
static void instr32_0FA4() { read_modrm_byte();
    int32_t data = read_write_e32(); write_e32(shld32(data, read_g32s(), read_op8() & 31));
}
static void instr16_0FA5() { read_modrm_byte();
    int32_t data = read_write_e16(); write_e16(shld16(data, read_g16(), reg8[CL] & 31));
}
static void instr32_0FA5() { read_modrm_byte();
    int32_t data = read_write_e32(); write_e32(shld32(data, read_g32s(), reg8[CL] & 31));
}

static void instr_0FA6() {
    // obsolete cmpxchg (os/2)
    trigger_ud();
}
static void instr_0FA7() { undefined_instruction(); }

static void instr16_0FA8() { push16(sreg[GS]); }
static void instr32_0FA8() { push32(sreg[GS]); }
static void instr16_0FA9() {
    switch_seg(GS, safe_read16(get_stack_pointer(0)));
    adjust_stack_reg(2);
}
static void instr32_0FA9() {
    switch_seg(GS, safe_read32s(get_stack_pointer(0)) & 0xFFFF);
    adjust_stack_reg(4);
}


static void instr_0FAA() {
    // rsm
    todo();
}

static void instr16_0FAB() { read_modrm_byte();
    if(modrm_byte[0] < 0xC0) {
        bts_mem(modrm_resolve(modrm_byte[0]), read_g16s());
    } else {
        write_reg_e16(bts_reg(read_reg_e16(), read_g16s() & 15));
    }
}
static void instr32_0FAB() { read_modrm_byte();
    if(modrm_byte[0] < 0xC0) {
        bts_mem(modrm_resolve(modrm_byte[0]), read_g32s());
    } else {
        write_reg_e32(bts_reg(read_reg_e32s(), read_g32s() & 31));
    }
}


static void instr16_0FAC() { read_modrm_byte();
    int32_t data = read_write_e16(); write_e16(shrd16(data, read_g16(), read_op8() & 31));
}
static void instr32_0FAC() { read_modrm_byte();
    int32_t data = read_write_e32(); write_e32(shrd32(data, read_g32s(), read_op8() & 31));
}
static void instr16_0FAD() { read_modrm_byte();
    int32_t data = read_write_e16(); write_e16(shrd16(data, read_g16(), reg8[CL] & 31));
}
static void instr32_0FAD() { read_modrm_byte();
    int32_t data = read_write_e32(); write_e32(shrd32(data, read_g32s(), reg8[CL] & 31));
}

static void instr_0FAE() { read_modrm_byte();
    // fxsave, fxrstor, ldmxcsr ...

    switch(modrm_byte[0] >> 3 & 7)
    {
        case 0: // fxsave
            if(modrm_byte[0] >= 0xC0) trigger_ud();
            {
                int32_t addr = modrm_resolve(modrm_byte[0]);
                fxsave(addr);
            }
            break;

        case 1: // fxrstor
            if(modrm_byte[0] >= 0xC0) trigger_ud();
            {
                int32_t addr = modrm_resolve(modrm_byte[0]);
                fxrstor(addr);
            }
            break;

        case 2: // ldmxcsr
            if(modrm_byte[0] >= 0xC0) trigger_ud();
            {
                int32_t addr = modrm_resolve(modrm_byte[0]);
                int32_t new_mxcsr = safe_read32s(addr);
                if(new_mxcsr & ~MXCSR_MASK)
                {
                    //dbg_log("Invalid mxcsr bits: " + h((new_mxcsr & ~MXCSR_MASK), 8));
                    assert(false);
                    trigger_gp(0);
                }
                *mxcsr = new_mxcsr;
            }
            break;

        case 3: // stmxcsr
            if(modrm_byte[0] >= 0xC0) trigger_ud();
            {
                int32_t addr = modrm_resolve(modrm_byte[0]);
                safe_write32(addr, *mxcsr);
            }
            break;

        case 5:
            // lfence
            dbg_assert_message(modrm_byte[0] >= 0xC0, "Unexpected mfence encoding");
            if(modrm_byte[0] < 0xC0) trigger_ud();
            break;

        case 6:
            // mfence
            dbg_assert_message(modrm_byte[0] >= 0xC0, "Unexpected mfence encoding");
            if(modrm_byte[0] < 0xC0) trigger_ud();
            break;

        case 7:
            // sfence or clflush
            dbg_assert_message(modrm_byte[0] >= 0xC0, "Unexpected sfence encoding");
            if(modrm_byte[0] < 0xC0) trigger_ud();
            break;

        default:
            //dbg_log("missing " + (modrm_byte[0] >> 3 & 7));
            todo();
    }
}

static void instr16_0FAF() { read_modrm_byte();
    int32_t data = read_e16s();
    write_g16(imul_reg16(read_g16s(), data));
}
static void instr32_0FAF() { read_modrm_byte();
    int32_t data = read_e32s();
    write_g32(imul_reg32(read_g32s(), data));
}


static void instr_0FB0() { read_modrm_byte();
    // cmpxchg8
    int32_t data = 0;
    int32_t virt_addr = 0;
    if(modrm_byte[0] < 0xC0)
    {
        virt_addr = modrm_resolve(modrm_byte[0]);
        writable_or_pagefault(virt_addr, 1);

        data = safe_read8(virt_addr);
    }
    else
        data = reg8[modrm_byte[0] << 2 & 0xC | modrm_byte[0] >> 2 & 1];


    cmp8(reg8[AL], data);

    if(getzf())
    {
        if(modrm_byte[0] < 0xC0)
            safe_write8(virt_addr, read_g8());
        else
            reg8[modrm_byte[0] << 2 & 0xC | modrm_byte[0] >> 2 & 1] = read_g8();
    }
    else
    {
        if(modrm_byte[0] < 0xC0)
            safe_write8(virt_addr, data);

        reg8[AL] = data;
    }
}
static void instr16_0FB1() { read_modrm_byte();
    // cmpxchg16/32
    int32_t data = 0;
    int32_t virt_addr = 0;
    if(modrm_byte[0] < 0xC0)
    {
        virt_addr = modrm_resolve(modrm_byte[0]);
        writable_or_pagefault(virt_addr, 2);

        data = safe_read16(virt_addr);
    }
    else
        data = read_reg_e16();

    cmp16(reg16[AX], data);

    if(getzf())
    {
        if(modrm_byte[0] < 0xC0)
            safe_write16(virt_addr, read_g16());
        else
            write_reg_e16(read_g16());
    }
    else
    {
        if(modrm_byte[0] < 0xC0)
            safe_write16(virt_addr, data);

        reg16[AX] = data;
    }
}
static void instr32_0FB1() { read_modrm_byte();
    int32_t virt_addr = 0;
    int32_t data = 0;
    if(modrm_byte[0] < 0xC0)
    {
        virt_addr = modrm_resolve(modrm_byte[0]);
        writable_or_pagefault(virt_addr, 4);

        data = safe_read32s(virt_addr);
    }
    else
    {
        data = read_reg_e32s();
    }

    cmp32(reg32s[EAX], data);

    if(getzf())
    {
        if(modrm_byte[0] < 0xC0)
            safe_write32(virt_addr, read_g32s());
        else
            write_reg_e32(read_g32s());
    }
    else
    {
        if(modrm_byte[0] < 0xC0)
            safe_write32(virt_addr, data);

        reg32s[EAX] = data;
    }
}

// lss
static void instr16_0FB2() { read_modrm_byte();
    if(modrm_byte[0] >= 0xC0) trigger_ud();
    lss16(modrm_resolve(*modrm_byte), *modrm_byte >> 2 & 14, SS);
}
static void instr32_0FB2() { read_modrm_byte();
    if(modrm_byte[0] >= 0xC0) trigger_ud();
    lss32(modrm_resolve(*modrm_byte), *modrm_byte >> 3 & 7, SS);
}

static void instr16_0FB3() { read_modrm_byte();
    if(modrm_byte[0] < 0xC0) {
        btr_mem(modrm_resolve(modrm_byte[0]), read_g16s());
    } else {
        write_reg_e16(btr_reg(read_reg_e16(), read_g16s() & 15));
    }
}
static void instr32_0FB3() { read_modrm_byte();
    if(modrm_byte[0] < 0xC0) {
        btr_mem(modrm_resolve(modrm_byte[0]), read_g32s());
    } else {
        write_reg_e32(btr_reg(read_reg_e32s(), read_g32s() & 31));
    }
}

// lfs, lgs
static void instr16_0FB4() { read_modrm_byte();
    if(modrm_byte[0] >= 0xC0) trigger_ud();
    lss16(modrm_resolve(*modrm_byte), *modrm_byte >> 2 & 14, FS);
}
static void instr32_0FB4() { read_modrm_byte();
    if(modrm_byte[0] >= 0xC0) trigger_ud();
    lss32(modrm_resolve(*modrm_byte), *modrm_byte >> 3 & 7, FS);
}
static void instr16_0FB5() { read_modrm_byte();
    if(modrm_byte[0] >= 0xC0) trigger_ud();
    lss16(modrm_resolve(*modrm_byte), *modrm_byte >> 2 & 14, GS);
}
static void instr32_0FB5() { read_modrm_byte();
    if(modrm_byte[0] >= 0xC0) trigger_ud();
    lss32(modrm_resolve(*modrm_byte), *modrm_byte >> 3 & 7, GS);
}

static void instr16_0FB6() { read_modrm_byte();
    // movzx
    int32_t data = read_e8();
    write_g16(data);
}
static void instr32_0FB6() { read_modrm_byte();
    int32_t data = read_e8();
    write_g32(data);
}

static void instr16_0FB7() { read_modrm_byte();
    // movzx
    int32_t data = read_e16();
    write_g16(data);
}
static void instr32_0FB7() { read_modrm_byte();
    int32_t data = read_e16();
    write_g32(data);
}

static void instr16_0FB8() { read_modrm_byte();
    // popcnt
    int32_t data = read_e16();
    write_g16(popcnt(data));
}
static void instr32_0FB8() { read_modrm_byte();
    int32_t data = read_e32s();
    write_g32(popcnt(data));
}

static void instr_0FB9() {
    // UD
    todo();
}

static void instr16_0FBA() { read_modrm_byte();
    switch(modrm_byte[0] >> 3 & 7)
    {
        case 4:
            if(modrm_byte[0] < 0xC0)
            {
                bt_mem(modrm_resolve(modrm_byte[0]), read_op8() & 15);
            }
            else
            {
                bt_reg(read_reg_e16(), read_op8() & 15);
            }
            break;
        case 5:
            if(modrm_byte[0] < 0xC0) {
                bts_mem(modrm_resolve(modrm_byte[0]), read_op8() & 15);
            } else {
                write_reg_e16(bts_reg(read_reg_e16(), read_op8() & 15));
            }
            break;
        case 6:
            if(modrm_byte[0] < 0xC0) {
                btr_mem(modrm_resolve(modrm_byte[0]), read_op8() & 15);
            } else {
                write_reg_e16(btr_reg(read_reg_e16(), read_op8() & 15));
            }
            break;
        case 7:
            if(modrm_byte[0] < 0xC0) {
                btc_mem(modrm_resolve(modrm_byte[0]), read_op8() & 15);
            } else {
                write_reg_e16(btc_reg(read_reg_e16(), read_op8() & 15));
            }
            break;
        default:
            dbg_log("%d", modrm_byte[0] >> 3 & 7);
            todo();
    }
}
static void instr32_0FBA() { read_modrm_byte();
    switch(modrm_byte[0] >> 3 & 7)
    {
        case 4:
            if(modrm_byte[0] < 0xC0)
            {
                bt_mem(modrm_resolve(modrm_byte[0]), read_op8() & 31);
            }
            else
            {
                bt_reg(read_reg_e32s(), read_op8() & 31);
            }
            break;
        case 5:
            if(modrm_byte[0] < 0xC0) {
                bts_mem(modrm_resolve(modrm_byte[0]), read_op8() & 31);
            } else {
                write_reg_e32(bts_reg(read_reg_e32s(), read_op8() & 31));
            }
            break;
        case 6:
            if(modrm_byte[0] < 0xC0) {
                btr_mem(modrm_resolve(modrm_byte[0]), read_op8() & 31);
            } else {
                write_reg_e32(btr_reg(read_reg_e32s(), read_op8() & 31));
            }
            break;
        case 7:
            if(modrm_byte[0] < 0xC0) {
                btc_mem(modrm_resolve(modrm_byte[0]), read_op8() & 31);
            } else {
                write_reg_e32(btc_reg(read_reg_e32s(), read_op8() & 31));
            }
            break;
        default:
            dbg_log("%d", modrm_byte[0] >> 3 & 7);
            todo();
    }
}

static void instr16_0FBB() { read_modrm_byte();
    if(modrm_byte[0] < 0xC0) {
        btc_mem(modrm_resolve(modrm_byte[0]), read_g16s());
    } else {
        write_reg_e16(btc_reg(read_reg_e16(), read_g16s() & 15));
    }
}
static void instr32_0FBB() { read_modrm_byte();
    if(modrm_byte[0] < 0xC0) {
        btc_mem(modrm_resolve(modrm_byte[0]), read_g32s());
    } else {
        write_reg_e32(btc_reg(read_reg_e32s(), read_g32s() & 31));
    }
}

static void instr16_0FBC() { read_modrm_byte();
    int32_t data = read_e16();
    write_g16(bsf16(read_g16(), data));
}
static void instr32_0FBC() { read_modrm_byte();
    int32_t data = read_e32s();
    write_g32(bsf32(read_g32s(), data));
}

static void instr16_0FBD() { read_modrm_byte();
    int32_t data = read_e16();
    write_g16(bsr16(read_g16(), data));
}
static void instr32_0FBD() { read_modrm_byte();
    int32_t data = read_e32s();
    write_g32(bsr32(read_g32s(), data));
}

static void instr16_0FBE() { read_modrm_byte();
    // movsx
    int32_t data = read_e8s();
    write_g16(data);
}
static void instr32_0FBE() { read_modrm_byte();
    int32_t data = read_e8s();
    write_g32(data);
}

static void instr16_0FBF() { read_modrm_byte();
    // movsx
    int32_t data = read_e16();
    write_g16(data);
}

static void instr32_0FBF() { read_modrm_byte();
    int32_t data = read_e16s();
    write_g32(data);
}

static void instr_0FC0() { read_modrm_byte();
    int32_t data = read_write_e8(); write_e8(xadd8(data, modrm_byte[0] >> 1 & 0xC | modrm_byte[0] >> 5 & 1));
}

static void instr16_0FC1() { read_modrm_byte();
    int32_t data = read_write_e16();
    write_e16(xadd16(data, modrm_byte[0] >> 2 & 14));
}
static void instr32_0FC1() { read_modrm_byte();
    int32_t data = read_write_e32();
    write_e32(xadd32(data, modrm_byte[0] >> 3 & 7));
}


static void instr_0FC2() { unimplemented_sse(); }
static void instr_0FC3() { unimplemented_sse(); }
static void instr_0FC4() { unimplemented_sse(); }
static void instr_0FC5() { unimplemented_sse(); }
static void instr_660FC5() { unimplemented_sse(); }
static void instr_0FC6() { unimplemented_sse(); }

static void instr_0FC7() {
    read_modrm_byte();

    switch(modrm_byte[0] >> 3 & 7)
    {
        case 1:
            // cmpxchg8b
            if(modrm_byte[0] >= 0xC0)
            {
                trigger_ud();
            }

            int32_t addr = modrm_resolve(modrm_byte[0]);
            writable_or_pagefault(addr, 8);

            int32_t m64_low = safe_read32s(addr);
            int32_t m64_high = safe_read32s(addr + 4);

            if(reg32s[EAX] == m64_low &&
               reg32s[EDX] == m64_high)
            {
                flags[0] |= FLAG_ZERO;

                safe_write32(addr, reg32s[EBX]);
                safe_write32(addr + 4, reg32s[ECX]);
            }
            else
            {
                flags[0] &= ~FLAG_ZERO;

                reg32s[EAX] = m64_low;
                reg32s[EDX] = m64_high;

                safe_write32(addr, m64_low);
                safe_write32(addr + 4, m64_high);
            }

            flags_changed[0] &= ~FLAG_ZERO;
            break;

        case 6:
            {
                int32_t has_rand = has_rand_int();

                int32_t rand = 0;
                if(has_rand)
                {
                    rand = get_rand_int();
                }
                //dbg_log("rdrand -> " + h(rand, 8));

                if(is_osize_32())
                {
                    set_e32(rand);
                }
                else
                {
                    set_e16(rand);
                }

                flags[0] &= ~FLAGS_ALL;
                flags[0] |= has_rand;
                flags_changed[0] = 0;
            }
            break;

        default:
            dbg_log("%d", modrm_byte[0] >> 3 & 7);
            todo();
    }
}

static void instr_0FC8() { bswap(EAX); }
static void instr_0FC9() { bswap(ECX); }
static void instr_0FCA() { bswap(EDX); }
static void instr_0FCB() { bswap(EBX); }
static void instr_0FCC() { bswap(ESP); }
static void instr_0FCD() { bswap(EBP); }
static void instr_0FCE() { bswap(ESI); }
static void instr_0FCF() { bswap(EDI); }

static void instr_0FD0() { unimplemented_sse(); }
static void instr_0FD1() { unimplemented_sse(); }
static void instr_0FD2() { unimplemented_sse(); }
static void instr_0FD3() { unimplemented_sse(); }
static void instr_660FD3() { unimplemented_sse(); }
static void instr_0FD4() { unimplemented_sse(); }
static void instr_0FD5() { unimplemented_sse(); }
static void instr_660FD5() { unimplemented_sse(); }
static void instr_0FD6() { unimplemented_sse(); }
static void instr_660FD6() {
    // movq xmm/m64, xmm
    task_switch_test_mmx();
    read_modrm_byte();
    assert(*modrm_byte < 0xC0);
    union reg64 data = read_xmm64s();
    int32_t addr = modrm_resolve(*modrm_byte);
    safe_write64(addr, data.u32[0], data.u32[1]);
}
static void instr_0FD7() { unimplemented_sse(); }
static void instr_660FD7() {
    // pmovmskb reg, xmm
    task_switch_test_mmx();
    read_modrm_byte();
    if(*modrm_byte < 0xC0) trigger_ud();

    union reg128 x = read_xmm_mem128s();
    int32_t result =
        x.u8[0] >> 7 << 0 | x.u8[1] >> 7 << 1 | x.u8[2] >> 7 << 2 | x.u8[3] >> 7 << 3 |
        x.u8[4] >> 7 << 4 | x.u8[5] >> 7 << 5 | x.u8[6] >> 7 << 6 | x.u8[7] >> 7 << 7 |
        x.u8[8] >> 7 << 8 | x.u8[9] >> 7 << 9 | x.u8[10] >> 7 << 10 | x.u8[11] >> 7 << 11 |
        x.u8[12] >> 7 << 12 | x.u8[13] >> 7 << 13 | x.u8[14] >> 7 << 14 | x.u8[15] >> 7 << 15;
    write_g32(result);
}

static void instr_0FD8() { unimplemented_sse(); }
static void instr_0FD9() { unimplemented_sse(); }
static void instr_0FDA() { unimplemented_sse(); }
static void instr_660FDA() { unimplemented_sse(); }
static void instr_0FDB() { unimplemented_sse(); }
static void instr_0FDC() { unimplemented_sse(); }
static void instr_660FDC() { unimplemented_sse(); }
static void instr_0FDD() { unimplemented_sse(); }
static void instr_660FDD() { unimplemented_sse(); }
static void instr_0FDE() { unimplemented_sse(); }
static void instr_660FDE() { unimplemented_sse(); }
static void instr_0FDF() { unimplemented_sse(); }

static void instr_0FE0() { unimplemented_sse(); }
static void instr_0FE1() { unimplemented_sse(); }
static void instr_0FE2() { unimplemented_sse(); }
static void instr_0FE3() { unimplemented_sse(); }
static void instr_0FE4() { unimplemented_sse(); }
static void instr_660FE4() { unimplemented_sse(); }
static void instr_0FE5() { unimplemented_sse(); }
static void instr_0FE6() { unimplemented_sse(); }
static void instr_0FE7() { unimplemented_sse(); }
static void instr_660FE7() {
    // movntdq m128, xmm
    task_switch_test_mmx();
    read_modrm_byte();

    if(*modrm_byte >= 0xC0) trigger_ud();

    union reg128 data = read_xmm128s();
    int32_t addr = modrm_resolve(*modrm_byte);
    safe_write128(addr, data);
}

static void instr_0FE8() { unimplemented_sse(); }
static void instr_0FE9() { unimplemented_sse(); }
static void instr_0FEA() { unimplemented_sse(); }
static void instr_0FEB() { unimplemented_sse(); }
static void instr_660FEB() { unimplemented_sse(); }
static void instr_0FEC() { unimplemented_sse(); }
static void instr_0FED() { unimplemented_sse(); }
static void instr_0FEE() { unimplemented_sse(); }
static void instr_0FEF() { unimplemented_sse(); }
static void instr_660FEF() {
    // pxor xmm, xmm/m128
    task_switch_test_mmx();
    read_modrm_byte();

    union reg128 source = read_xmm_mem128s();
    union reg128 destination = read_xmm128s();

    write_xmm128s(
        source.u32[0] ^ destination.u32[0],
        source.u32[1] ^ destination.u32[1],
        source.u32[2] ^ destination.u32[2],
        source.u32[3] ^ destination.u32[3]
    );
}

static void instr_0FF0() { unimplemented_sse(); }
static void instr_0FF1() { unimplemented_sse(); }
static void instr_0FF2() { unimplemented_sse(); }
static void instr_0FF3() { unimplemented_sse(); }
static void instr_0FF4() { unimplemented_sse(); }
static void instr_0FF5() { unimplemented_sse(); }
static void instr_0FF6() { unimplemented_sse(); }
static void instr_0FF7() { unimplemented_sse(); }

static void instr_0FF8() { unimplemented_sse(); }
static void instr_0FF9() { unimplemented_sse(); }
static void instr_0FFA() { unimplemented_sse(); }
static void instr_660FFA() { unimplemented_sse(); }
static void instr_0FFB() { unimplemented_sse(); }
static void instr_0FFC() { unimplemented_sse(); }
static void instr_0FFD() { unimplemented_sse(); }
static void instr_0FFE() { unimplemented_sse(); }

static void instr_0FFF() {
    // Windows 98
    dbg_log("#ud: 0F FF");
    trigger_ud();
}


static void run_instruction0f_16(int32_t opcode)
{
    // XXX: This table is generated. Don't modify
    switch(opcode)
    {
case 0x00:
    instr_0F00();
    break;
case 0x01:
    instr_0F01();
    break;
case 0x02:
    instr16_0F02();
    break;
case 0x03:
    instr16_0F03();
    break;
case 0x04:
    instr_0F04();
    break;
case 0x05:
    instr_0F05();
    break;
case 0x06:
    instr_0F06();
    break;
case 0x07:
    instr_0F07();
    break;
case 0x08:
    instr_0F08();
    break;
case 0x09:
    instr_0F09();
    break;
case 0x0A:
    instr_0F0A();
    break;
case 0x0B:
    instr_0F0B();
    break;
case 0x0C:
    instr_0F0C();
    break;
case 0x0D:
    instr_0F0D();
    break;
case 0x0E:
    instr_0F0E();
    break;
case 0x0F:
    instr_0F0F();
    break;
case 0x10:
    instr_0F10();
    break;
case 0x11:
    instr_0F11();
    break;
case 0x12:
    (*prefixes & PREFIX_66) ? instr_660F12() :
    instr_0F12();
    break;
case 0x13:
    (*prefixes & PREFIX_66) ? instr_660F13() :
    instr_0F13();
    break;
case 0x14:
    (*prefixes & PREFIX_66) ? instr_660F14() :
    instr_0F14();
    break;
case 0x15:
    instr_0F15();
    break;
case 0x16:
    instr_0F16();
    break;
case 0x17:
    instr_0F17();
    break;
case 0x18:
    instr_0F18();
    break;
case 0x19:
    instr_0F19();
    break;
case 0x1A:
    instr_0F1A();
    break;
case 0x1B:
    instr_0F1B();
    break;
case 0x1C:
    instr_0F1C();
    break;
case 0x1D:
    instr_0F1D();
    break;
case 0x1E:
    instr_0F1E();
    break;
case 0x1F:
    instr_0F1F();
    break;
case 0x20:
    instr_0F20();
    break;
case 0x21:
    instr_0F21();
    break;
case 0x22:
    instr_0F22();
    break;
case 0x23:
    instr_0F23();
    break;
case 0x24:
    instr_0F24();
    break;
case 0x25:
    instr_0F25();
    break;
case 0x26:
    instr_0F26();
    break;
case 0x27:
    instr_0F27();
    break;
case 0x28:
    (*prefixes & PREFIX_66) ? instr_660F28() :
    instr_0F28();
    break;
case 0x29:
    (*prefixes & PREFIX_66) ? instr_660F29() :
    instr_0F29();
    break;
case 0x2A:
    instr_0F2A();
    break;
case 0x2B:
    (*prefixes & PREFIX_66) ? instr_660F2B() :
    instr_0F2B();
    break;
case 0x2C:
    (*prefixes & PREFIX_F2) ? instr_F20F2C() :
    instr_0F2C();
    break;
case 0x2D:
    instr_0F2D();
    break;
case 0x2E:
    instr_0F2E();
    break;
case 0x2F:
    instr_0F2F();
    break;
case 0x30:
    instr_0F30();
    break;
case 0x31:
    instr_0F31();
    break;
case 0x32:
    instr_0F32();
    break;
case 0x33:
    instr_0F33();
    break;
case 0x34:
    instr_0F34();
    break;
case 0x35:
    instr_0F35();
    break;
case 0x36:
    instr_0F36();
    break;
case 0x37:
    instr_0F37();
    break;
case 0x38:
    instr_0F38();
    break;
case 0x39:
    instr_0F39();
    break;
case 0x3A:
    instr_0F3A();
    break;
case 0x3B:
    instr_0F3B();
    break;
case 0x3C:
    instr_0F3C();
    break;
case 0x3D:
    instr_0F3D();
    break;
case 0x3E:
    instr_0F3E();
    break;
case 0x3F:
    instr_0F3F();
    break;
case 0x40:
    instr16_0F40();
    break;
case 0x41:
    instr16_0F41();
    break;
case 0x42:
    instr16_0F42();
    break;
case 0x43:
    instr16_0F43();
    break;
case 0x44:
    instr16_0F44();
    break;
case 0x45:
    instr16_0F45();
    break;
case 0x46:
    instr16_0F46();
    break;
case 0x47:
    instr16_0F47();
    break;
case 0x48:
    instr16_0F48();
    break;
case 0x49:
    instr16_0F49();
    break;
case 0x4A:
    instr16_0F4A();
    break;
case 0x4B:
    instr16_0F4B();
    break;
case 0x4C:
    instr16_0F4C();
    break;
case 0x4D:
    instr16_0F4D();
    break;
case 0x4E:
    instr16_0F4E();
    break;
case 0x4F:
    instr16_0F4F();
    break;
case 0x50:
    instr_0F50();
    break;
case 0x51:
    instr_0F51();
    break;
case 0x52:
    instr_0F52();
    break;
case 0x53:
    instr_0F53();
    break;
case 0x54:
    (*prefixes & PREFIX_66) ? instr_660F54() :
    instr_0F54();
    break;
case 0x55:
    instr_0F55();
    break;
case 0x56:
    instr_0F56();
    break;
case 0x57:
    (*prefixes & PREFIX_66) ? instr_660F57() :
    instr_0F57();
    break;
case 0x58:
    instr_0F58();
    break;
case 0x59:
    instr_0F59();
    break;
case 0x5A:
    instr_0F5A();
    break;
case 0x5B:
    instr_0F5B();
    break;
case 0x5C:
    instr_0F5C();
    break;
case 0x5D:
    instr_0F5D();
    break;
case 0x5E:
    instr_0F5E();
    break;
case 0x5F:
    instr_0F5F();
    break;
case 0x60:
    (*prefixes & PREFIX_66) ? instr_660F60() :
    instr_0F60();
    break;
case 0x61:
    (*prefixes & PREFIX_66) ? instr_660F61() :
    instr_0F61();
    break;
case 0x62:
    instr_0F62();
    break;
case 0x63:
    instr_0F63();
    break;
case 0x64:
    instr_0F64();
    break;
case 0x65:
    instr_0F65();
    break;
case 0x66:
    instr_0F66();
    break;
case 0x67:
    (*prefixes & PREFIX_66) ? instr_660F67() :
    instr_0F67();
    break;
case 0x68:
    (*prefixes & PREFIX_66) ? instr_660F68() :
    instr_0F68();
    break;
case 0x69:
    instr_0F69();
    break;
case 0x6A:
    instr_0F6A();
    break;
case 0x6B:
    instr_0F6B();
    break;
case 0x6C:
    instr_0F6C();
    break;
case 0x6D:
    instr_0F6D();
    break;
case 0x6E:
    (*prefixes & PREFIX_66) ? instr_660F6E() :
    instr_0F6E();
    break;
case 0x6F:
    (*prefixes & PREFIX_66) ? instr_660F6F() :
    (*prefixes & PREFIX_F3) ? instr_F30F6F() :
    instr_0F6F();
    break;
case 0x70:
    (*prefixes & PREFIX_66) ? instr_660F70() :
    (*prefixes & PREFIX_F2) ? instr_F20F70() :
    (*prefixes & PREFIX_F3) ? instr_F30F70() :
    instr_0F70();
    break;
case 0x71:
    instr_0F71();
    break;
case 0x72:
    instr_0F72();
    break;
case 0x73:
    (*prefixes & PREFIX_66) ? instr_660F73() :
    instr_0F73();
    break;
case 0x74:
    (*prefixes & PREFIX_66) ? instr_660F74() :
    instr_0F74();
    break;
case 0x75:
    (*prefixes & PREFIX_66) ? instr_660F75() :
    instr_0F75();
    break;
case 0x76:
    (*prefixes & PREFIX_66) ? instr_660F76() :
    instr_0F76();
    break;
case 0x77:
    instr_0F77();
    break;
case 0x78:
    instr_0F78();
    break;
case 0x79:
    instr_0F79();
    break;
case 0x7A:
    instr_0F7A();
    break;
case 0x7B:
    instr_0F7B();
    break;
case 0x7C:
    instr_0F7C();
    break;
case 0x7D:
    instr_0F7D();
    break;
case 0x7E:
    (*prefixes & PREFIX_66) ? instr_660F7E() :
    (*prefixes & PREFIX_F3) ? instr_F30F7E() :
    instr_0F7E();
    break;
case 0x7F:
    (*prefixes & PREFIX_66) ? instr_660F7F() :
    (*prefixes & PREFIX_F3) ? instr_F30F7F() :
    instr_0F7F();
    break;
case 0x80:
    instr16_0F80();
    break;
case 0x81:
    instr16_0F81();
    break;
case 0x82:
    instr16_0F82();
    break;
case 0x83:
    instr16_0F83();
    break;
case 0x84:
    instr16_0F84();
    break;
case 0x85:
    instr16_0F85();
    break;
case 0x86:
    instr16_0F86();
    break;
case 0x87:
    instr16_0F87();
    break;
case 0x88:
    instr16_0F88();
    break;
case 0x89:
    instr16_0F89();
    break;
case 0x8A:
    instr16_0F8A();
    break;
case 0x8B:
    instr16_0F8B();
    break;
case 0x8C:
    instr16_0F8C();
    break;
case 0x8D:
    instr16_0F8D();
    break;
case 0x8E:
    instr16_0F8E();
    break;
case 0x8F:
    instr16_0F8F();
    break;
case 0x90:
    instr_0F90();
    break;
case 0x91:
    instr_0F91();
    break;
case 0x92:
    instr_0F92();
    break;
case 0x93:
    instr_0F93();
    break;
case 0x94:
    instr_0F94();
    break;
case 0x95:
    instr_0F95();
    break;
case 0x96:
    instr_0F96();
    break;
case 0x97:
    instr_0F97();
    break;
case 0x98:
    instr_0F98();
    break;
case 0x99:
    instr_0F99();
    break;
case 0x9A:
    instr_0F9A();
    break;
case 0x9B:
    instr_0F9B();
    break;
case 0x9C:
    instr_0F9C();
    break;
case 0x9D:
    instr_0F9D();
    break;
case 0x9E:
    instr_0F9E();
    break;
case 0x9F:
    instr_0F9F();
    break;
case 0xA0:
    instr16_0FA0();
    break;
case 0xA1:
    instr16_0FA1();
    break;
case 0xA2:
    instr_0FA2();
    break;
case 0xA3:
    instr16_0FA3();
    break;
case 0xA4:
    instr16_0FA4();
    break;
case 0xA5:
    instr16_0FA5();
    break;
case 0xA6:
    instr_0FA6();
    break;
case 0xA7:
    instr_0FA7();
    break;
case 0xA8:
    instr16_0FA8();
    break;
case 0xA9:
    instr16_0FA9();
    break;
case 0xAA:
    instr_0FAA();
    break;
case 0xAB:
    instr16_0FAB();
    break;
case 0xAC:
    instr16_0FAC();
    break;
case 0xAD:
    instr16_0FAD();
    break;
case 0xAE:
    instr_0FAE();
    break;
case 0xAF:
    instr16_0FAF();
    break;
case 0xB0:
    instr_0FB0();
    break;
case 0xB1:
    instr16_0FB1();
    break;
case 0xB2:
    instr16_0FB2();
    break;
case 0xB3:
    instr16_0FB3();
    break;
case 0xB4:
    instr16_0FB4();
    break;
case 0xB5:
    instr16_0FB5();
    break;
case 0xB6:
    instr16_0FB6();
    break;
case 0xB7:
    instr16_0FB7();
    break;
case 0xB8:
    instr16_0FB8();
    break;
case 0xB9:
    instr_0FB9();
    break;
case 0xBA:
    instr16_0FBA();
    break;
case 0xBB:
    instr16_0FBB();
    break;
case 0xBC:
    instr16_0FBC();
    break;
case 0xBD:
    instr16_0FBD();
    break;
case 0xBE:
    instr16_0FBE();
    break;
case 0xBF:
    instr16_0FBF();
    break;
case 0xC0:
    instr_0FC0();
    break;
case 0xC1:
    instr16_0FC1();
    break;
case 0xC2:
    instr_0FC2();
    break;
case 0xC3:
    instr_0FC3();
    break;
case 0xC4:
    instr_0FC4();
    break;
case 0xC5:
    (*prefixes & PREFIX_66) ? instr_660FC5() :
    instr_0FC5();
    break;
case 0xC6:
    instr_0FC6();
    break;
case 0xC7:
    instr_0FC7();
    break;
case 0xC8:
    instr_0FC8();
    break;
case 0xC9:
    instr_0FC9();
    break;
case 0xCA:
    instr_0FCA();
    break;
case 0xCB:
    instr_0FCB();
    break;
case 0xCC:
    instr_0FCC();
    break;
case 0xCD:
    instr_0FCD();
    break;
case 0xCE:
    instr_0FCE();
    break;
case 0xCF:
    instr_0FCF();
    break;
case 0xD0:
    instr_0FD0();
    break;
case 0xD1:
    instr_0FD1();
    break;
case 0xD2:
    instr_0FD2();
    break;
case 0xD3:
    (*prefixes & PREFIX_66) ? instr_660FD3() :
    instr_0FD3();
    break;
case 0xD4:
    instr_0FD4();
    break;
case 0xD5:
    (*prefixes & PREFIX_66) ? instr_660FD5() :
    instr_0FD5();
    break;
case 0xD6:
    (*prefixes & PREFIX_66) ? instr_660FD6() :
    instr_0FD6();
    break;
case 0xD7:
    (*prefixes & PREFIX_66) ? instr_660FD7() :
    instr_0FD7();
    break;
case 0xD8:
    instr_0FD8();
    break;
case 0xD9:
    instr_0FD9();
    break;
case 0xDA:
    (*prefixes & PREFIX_66) ? instr_660FDA() :
    instr_0FDA();
    break;
case 0xDB:
    instr_0FDB();
    break;
case 0xDC:
    (*prefixes & PREFIX_66) ? instr_660FDC() :
    instr_0FDC();
    break;
case 0xDD:
    (*prefixes & PREFIX_66) ? instr_660FDD() :
    instr_0FDD();
    break;
case 0xDE:
    (*prefixes & PREFIX_66) ? instr_660FDE() :
    instr_0FDE();
    break;
case 0xDF:
    instr_0FDF();
    break;
case 0xE0:
    instr_0FE0();
    break;
case 0xE1:
    instr_0FE1();
    break;
case 0xE2:
    instr_0FE2();
    break;
case 0xE3:
    instr_0FE3();
    break;
case 0xE4:
    (*prefixes & PREFIX_66) ? instr_660FE4() :
    instr_0FE4();
    break;
case 0xE5:
    instr_0FE5();
    break;
case 0xE6:
    instr_0FE6();
    break;
case 0xE7:
    (*prefixes & PREFIX_66) ? instr_660FE7() :
    instr_0FE7();
    break;
case 0xE8:
    instr_0FE8();
    break;
case 0xE9:
    instr_0FE9();
    break;
case 0xEA:
    instr_0FEA();
    break;
case 0xEB:
    (*prefixes & PREFIX_66) ? instr_660FEB() :
    instr_0FEB();
    break;
case 0xEC:
    instr_0FEC();
    break;
case 0xED:
    instr_0FED();
    break;
case 0xEE:
    instr_0FEE();
    break;
case 0xEF:
    (*prefixes & PREFIX_66) ? instr_660FEF() :
    instr_0FEF();
    break;
case 0xF0:
    instr_0FF0();
    break;
case 0xF1:
    instr_0FF1();
    break;
case 0xF2:
    instr_0FF2();
    break;
case 0xF3:
    instr_0FF3();
    break;
case 0xF4:
    instr_0FF4();
    break;
case 0xF5:
    instr_0FF5();
    break;
case 0xF6:
    instr_0FF6();
    break;
case 0xF7:
    instr_0FF7();
    break;
case 0xF8:
    instr_0FF8();
    break;
case 0xF9:
    instr_0FF9();
    break;
case 0xFA:
    (*prefixes & PREFIX_66) ? instr_660FFA() :
    instr_0FFA();
    break;
case 0xFB:
    instr_0FFB();
    break;
case 0xFC:
    instr_0FFC();
    break;
case 0xFD:
    instr_0FFD();
    break;
case 0xFE:
    instr_0FFE();
    break;
case 0xFF:
    instr_0FFF();
    break;
default: assert(false);
    }
}

static void run_instruction0f_32(int32_t opcode)
{
    // XXX: This table is generated. Don't modify
    switch(opcode)
    {
case 0x00:
    instr_0F00();
    break;
case 0x01:
    instr_0F01();
    break;
case 0x02:
    instr32_0F02();
    break;
case 0x03:
    instr32_0F03();
    break;
case 0x04:
    instr_0F04();
    break;
case 0x05:
    instr_0F05();
    break;
case 0x06:
    instr_0F06();
    break;
case 0x07:
    instr_0F07();
    break;
case 0x08:
    instr_0F08();
    break;
case 0x09:
    instr_0F09();
    break;
case 0x0A:
    instr_0F0A();
    break;
case 0x0B:
    instr_0F0B();
    break;
case 0x0C:
    instr_0F0C();
    break;
case 0x0D:
    instr_0F0D();
    break;
case 0x0E:
    instr_0F0E();
    break;
case 0x0F:
    instr_0F0F();
    break;
case 0x10:
    instr_0F10();
    break;
case 0x11:
    instr_0F11();
    break;
case 0x12:
    (*prefixes & PREFIX_66) ? instr_660F12() :
    instr_0F12();
    break;
case 0x13:
    (*prefixes & PREFIX_66) ? instr_660F13() :
    instr_0F13();
    break;
case 0x14:
    (*prefixes & PREFIX_66) ? instr_660F14() :
    instr_0F14();
    break;
case 0x15:
    instr_0F15();
    break;
case 0x16:
    instr_0F16();
    break;
case 0x17:
    instr_0F17();
    break;
case 0x18:
    instr_0F18();
    break;
case 0x19:
    instr_0F19();
    break;
case 0x1A:
    instr_0F1A();
    break;
case 0x1B:
    instr_0F1B();
    break;
case 0x1C:
    instr_0F1C();
    break;
case 0x1D:
    instr_0F1D();
    break;
case 0x1E:
    instr_0F1E();
    break;
case 0x1F:
    instr_0F1F();
    break;
case 0x20:
    instr_0F20();
    break;
case 0x21:
    instr_0F21();
    break;
case 0x22:
    instr_0F22();
    break;
case 0x23:
    instr_0F23();
    break;
case 0x24:
    instr_0F24();
    break;
case 0x25:
    instr_0F25();
    break;
case 0x26:
    instr_0F26();
    break;
case 0x27:
    instr_0F27();
    break;
case 0x28:
    (*prefixes & PREFIX_66) ? instr_660F28() :
    instr_0F28();
    break;
case 0x29:
    (*prefixes & PREFIX_66) ? instr_660F29() :
    instr_0F29();
    break;
case 0x2A:
    instr_0F2A();
    break;
case 0x2B:
    (*prefixes & PREFIX_66) ? instr_660F2B() :
    instr_0F2B();
    break;
case 0x2C:
    (*prefixes & PREFIX_F2) ? instr_F20F2C() :
    instr_0F2C();
    break;
case 0x2D:
    instr_0F2D();
    break;
case 0x2E:
    instr_0F2E();
    break;
case 0x2F:
    instr_0F2F();
    break;
case 0x30:
    instr_0F30();
    break;
case 0x31:
    instr_0F31();
    break;
case 0x32:
    instr_0F32();
    break;
case 0x33:
    instr_0F33();
    break;
case 0x34:
    instr_0F34();
    break;
case 0x35:
    instr_0F35();
    break;
case 0x36:
    instr_0F36();
    break;
case 0x37:
    instr_0F37();
    break;
case 0x38:
    instr_0F38();
    break;
case 0x39:
    instr_0F39();
    break;
case 0x3A:
    instr_0F3A();
    break;
case 0x3B:
    instr_0F3B();
    break;
case 0x3C:
    instr_0F3C();
    break;
case 0x3D:
    instr_0F3D();
    break;
case 0x3E:
    instr_0F3E();
    break;
case 0x3F:
    instr_0F3F();
    break;
case 0x40:
    instr32_0F40();
    break;
case 0x41:
    instr32_0F41();
    break;
case 0x42:
    instr32_0F42();
    break;
case 0x43:
    instr32_0F43();
    break;
case 0x44:
    instr32_0F44();
    break;
case 0x45:
    instr32_0F45();
    break;
case 0x46:
    instr32_0F46();
    break;
case 0x47:
    instr32_0F47();
    break;
case 0x48:
    instr32_0F48();
    break;
case 0x49:
    instr32_0F49();
    break;
case 0x4A:
    instr32_0F4A();
    break;
case 0x4B:
    instr32_0F4B();
    break;
case 0x4C:
    instr32_0F4C();
    break;
case 0x4D:
    instr32_0F4D();
    break;
case 0x4E:
    instr32_0F4E();
    break;
case 0x4F:
    instr32_0F4F();
    break;
case 0x50:
    instr_0F50();
    break;
case 0x51:
    instr_0F51();
    break;
case 0x52:
    instr_0F52();
    break;
case 0x53:
    instr_0F53();
    break;
case 0x54:
    (*prefixes & PREFIX_66) ? instr_660F54() :
    instr_0F54();
    break;
case 0x55:
    instr_0F55();
    break;
case 0x56:
    instr_0F56();
    break;
case 0x57:
    (*prefixes & PREFIX_66) ? instr_660F57() :
    instr_0F57();
    break;
case 0x58:
    instr_0F58();
    break;
case 0x59:
    instr_0F59();
    break;
case 0x5A:
    instr_0F5A();
    break;
case 0x5B:
    instr_0F5B();
    break;
case 0x5C:
    instr_0F5C();
    break;
case 0x5D:
    instr_0F5D();
    break;
case 0x5E:
    instr_0F5E();
    break;
case 0x5F:
    instr_0F5F();
    break;
case 0x60:
    (*prefixes & PREFIX_66) ? instr_660F60() :
    instr_0F60();
    break;
case 0x61:
    (*prefixes & PREFIX_66) ? instr_660F61() :
    instr_0F61();
    break;
case 0x62:
    instr_0F62();
    break;
case 0x63:
    instr_0F63();
    break;
case 0x64:
    instr_0F64();
    break;
case 0x65:
    instr_0F65();
    break;
case 0x66:
    instr_0F66();
    break;
case 0x67:
    (*prefixes & PREFIX_66) ? instr_660F67() :
    instr_0F67();
    break;
case 0x68:
    (*prefixes & PREFIX_66) ? instr_660F68() :
    instr_0F68();
    break;
case 0x69:
    instr_0F69();
    break;
case 0x6A:
    instr_0F6A();
    break;
case 0x6B:
    instr_0F6B();
    break;
case 0x6C:
    instr_0F6C();
    break;
case 0x6D:
    instr_0F6D();
    break;
case 0x6E:
    (*prefixes & PREFIX_66) ? instr_660F6E() :
    instr_0F6E();
    break;
case 0x6F:
    (*prefixes & PREFIX_66) ? instr_660F6F() :
    (*prefixes & PREFIX_F3) ? instr_F30F6F() :
    instr_0F6F();
    break;
case 0x70:
    (*prefixes & PREFIX_66) ? instr_660F70() :
    (*prefixes & PREFIX_F2) ? instr_F20F70() :
    (*prefixes & PREFIX_F3) ? instr_F30F70() :
    instr_0F70();
    break;
case 0x71:
    instr_0F71();
    break;
case 0x72:
    instr_0F72();
    break;
case 0x73:
    (*prefixes & PREFIX_66) ? instr_660F73() :
    instr_0F73();
    break;
case 0x74:
    (*prefixes & PREFIX_66) ? instr_660F74() :
    instr_0F74();
    break;
case 0x75:
    (*prefixes & PREFIX_66) ? instr_660F75() :
    instr_0F75();
    break;
case 0x76:
    (*prefixes & PREFIX_66) ? instr_660F76() :
    instr_0F76();
    break;
case 0x77:
    instr_0F77();
    break;
case 0x78:
    instr_0F78();
    break;
case 0x79:
    instr_0F79();
    break;
case 0x7A:
    instr_0F7A();
    break;
case 0x7B:
    instr_0F7B();
    break;
case 0x7C:
    instr_0F7C();
    break;
case 0x7D:
    instr_0F7D();
    break;
case 0x7E:
    (*prefixes & PREFIX_66) ? instr_660F7E() :
    (*prefixes & PREFIX_F3) ? instr_F30F7E() :
    instr_0F7E();
    break;
case 0x7F:
    (*prefixes & PREFIX_66) ? instr_660F7F() :
    (*prefixes & PREFIX_F3) ? instr_F30F7F() :
    instr_0F7F();
    break;
case 0x80:
    instr32_0F80();
    break;
case 0x81:
    instr32_0F81();
    break;
case 0x82:
    instr32_0F82();
    break;
case 0x83:
    instr32_0F83();
    break;
case 0x84:
    instr32_0F84();
    break;
case 0x85:
    instr32_0F85();
    break;
case 0x86:
    instr32_0F86();
    break;
case 0x87:
    instr32_0F87();
    break;
case 0x88:
    instr32_0F88();
    break;
case 0x89:
    instr32_0F89();
    break;
case 0x8A:
    instr32_0F8A();
    break;
case 0x8B:
    instr32_0F8B();
    break;
case 0x8C:
    instr32_0F8C();
    break;
case 0x8D:
    instr32_0F8D();
    break;
case 0x8E:
    instr32_0F8E();
    break;
case 0x8F:
    instr32_0F8F();
    break;
case 0x90:
    instr_0F90();
    break;
case 0x91:
    instr_0F91();
    break;
case 0x92:
    instr_0F92();
    break;
case 0x93:
    instr_0F93();
    break;
case 0x94:
    instr_0F94();
    break;
case 0x95:
    instr_0F95();
    break;
case 0x96:
    instr_0F96();
    break;
case 0x97:
    instr_0F97();
    break;
case 0x98:
    instr_0F98();
    break;
case 0x99:
    instr_0F99();
    break;
case 0x9A:
    instr_0F9A();
    break;
case 0x9B:
    instr_0F9B();
    break;
case 0x9C:
    instr_0F9C();
    break;
case 0x9D:
    instr_0F9D();
    break;
case 0x9E:
    instr_0F9E();
    break;
case 0x9F:
    instr_0F9F();
    break;
case 0xA0:
    instr32_0FA0();
    break;
case 0xA1:
    instr32_0FA1();
    break;
case 0xA2:
    instr_0FA2();
    break;
case 0xA3:
    instr32_0FA3();
    break;
case 0xA4:
    instr32_0FA4();
    break;
case 0xA5:
    instr32_0FA5();
    break;
case 0xA6:
    instr_0FA6();
    break;
case 0xA7:
    instr_0FA7();
    break;
case 0xA8:
    instr32_0FA8();
    break;
case 0xA9:
    instr32_0FA9();
    break;
case 0xAA:
    instr_0FAA();
    break;
case 0xAB:
    instr32_0FAB();
    break;
case 0xAC:
    instr32_0FAC();
    break;
case 0xAD:
    instr32_0FAD();
    break;
case 0xAE:
    instr_0FAE();
    break;
case 0xAF:
    instr32_0FAF();
    break;
case 0xB0:
    instr_0FB0();
    break;
case 0xB1:
    instr32_0FB1();
    break;
case 0xB2:
    instr32_0FB2();
    break;
case 0xB3:
    instr32_0FB3();
    break;
case 0xB4:
    instr32_0FB4();
    break;
case 0xB5:
    instr32_0FB5();
    break;
case 0xB6:
    instr32_0FB6();
    break;
case 0xB7:
    instr32_0FB7();
    break;
case 0xB8:
    instr32_0FB8();
    break;
case 0xB9:
    instr_0FB9();
    break;
case 0xBA:
    instr32_0FBA();
    break;
case 0xBB:
    instr32_0FBB();
    break;
case 0xBC:
    instr32_0FBC();
    break;
case 0xBD:
    instr32_0FBD();
    break;
case 0xBE:
    instr32_0FBE();
    break;
case 0xBF:
    instr32_0FBF();
    break;
case 0xC0:
    instr_0FC0();
    break;
case 0xC1:
    instr32_0FC1();
    break;
case 0xC2:
    instr_0FC2();
    break;
case 0xC3:
    instr_0FC3();
    break;
case 0xC4:
    instr_0FC4();
    break;
case 0xC5:
    (*prefixes & PREFIX_66) ? instr_660FC5() :
    instr_0FC5();
    break;
case 0xC6:
    instr_0FC6();
    break;
case 0xC7:
    instr_0FC7();
    break;
case 0xC8:
    instr_0FC8();
    break;
case 0xC9:
    instr_0FC9();
    break;
case 0xCA:
    instr_0FCA();
    break;
case 0xCB:
    instr_0FCB();
    break;
case 0xCC:
    instr_0FCC();
    break;
case 0xCD:
    instr_0FCD();
    break;
case 0xCE:
    instr_0FCE();
    break;
case 0xCF:
    instr_0FCF();
    break;
case 0xD0:
    instr_0FD0();
    break;
case 0xD1:
    instr_0FD1();
    break;
case 0xD2:
    instr_0FD2();
    break;
case 0xD3:
    (*prefixes & PREFIX_66) ? instr_660FD3() :
    instr_0FD3();
    break;
case 0xD4:
    instr_0FD4();
    break;
case 0xD5:
    (*prefixes & PREFIX_66) ? instr_660FD5() :
    instr_0FD5();
    break;
case 0xD6:
    (*prefixes & PREFIX_66) ? instr_660FD6() :
    instr_0FD6();
    break;
case 0xD7:
    (*prefixes & PREFIX_66) ? instr_660FD7() :
    instr_0FD7();
    break;
case 0xD8:
    instr_0FD8();
    break;
case 0xD9:
    instr_0FD9();
    break;
case 0xDA:
    (*prefixes & PREFIX_66) ? instr_660FDA() :
    instr_0FDA();
    break;
case 0xDB:
    instr_0FDB();
    break;
case 0xDC:
    (*prefixes & PREFIX_66) ? instr_660FDC() :
    instr_0FDC();
    break;
case 0xDD:
    (*prefixes & PREFIX_66) ? instr_660FDD() :
    instr_0FDD();
    break;
case 0xDE:
    (*prefixes & PREFIX_66) ? instr_660FDE() :
    instr_0FDE();
    break;
case 0xDF:
    instr_0FDF();
    break;
case 0xE0:
    instr_0FE0();
    break;
case 0xE1:
    instr_0FE1();
    break;
case 0xE2:
    instr_0FE2();
    break;
case 0xE3:
    instr_0FE3();
    break;
case 0xE4:
    (*prefixes & PREFIX_66) ? instr_660FE4() :
    instr_0FE4();
    break;
case 0xE5:
    instr_0FE5();
    break;
case 0xE6:
    instr_0FE6();
    break;
case 0xE7:
    (*prefixes & PREFIX_66) ? instr_660FE7() :
    instr_0FE7();
    break;
case 0xE8:
    instr_0FE8();
    break;
case 0xE9:
    instr_0FE9();
    break;
case 0xEA:
    instr_0FEA();
    break;
case 0xEB:
    (*prefixes & PREFIX_66) ? instr_660FEB() :
    instr_0FEB();
    break;
case 0xEC:
    instr_0FEC();
    break;
case 0xED:
    instr_0FED();
    break;
case 0xEE:
    instr_0FEE();
    break;
case 0xEF:
    (*prefixes & PREFIX_66) ? instr_660FEF() :
    instr_0FEF();
    break;
case 0xF0:
    instr_0FF0();
    break;
case 0xF1:
    instr_0FF1();
    break;
case 0xF2:
    instr_0FF2();
    break;
case 0xF3:
    instr_0FF3();
    break;
case 0xF4:
    instr_0FF4();
    break;
case 0xF5:
    instr_0FF5();
    break;
case 0xF6:
    instr_0FF6();
    break;
case 0xF7:
    instr_0FF7();
    break;
case 0xF8:
    instr_0FF8();
    break;
case 0xF9:
    instr_0FF9();
    break;
case 0xFA:
    (*prefixes & PREFIX_66) ? instr_660FFA() :
    instr_0FFA();
    break;
case 0xFB:
    instr_0FFB();
    break;
case 0xFC:
    instr_0FFC();
    break;
case 0xFD:
    instr_0FFD();
    break;
case 0xFE:
    instr_0FFE();
    break;
case 0xFF:
    instr_0FFF();
    break;
default: assert(false);
    }
}
