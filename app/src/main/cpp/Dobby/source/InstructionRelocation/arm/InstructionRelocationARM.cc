#include "platform_macro.h"

#if defined(TARGET_ARCH_ARM)

#include "dobby_internal.h"

#include "InstructionRelocation/arm/InstructionRelocationARM.h"

#include "core/arch/arm/registers-arm.h"
#include "core/assembler/assembler-arm.h"
#include "core/codegen/codegen-arm.h"

using namespace zz;
using namespace zz::arm;

typedef struct {
    addr_t mapped_addr;

    bool thumb_mode;

    uint8_t *buffer;
    uint8_t *buffer_cursor;
    size_t buffer_size;

    vmaddr_t src_vmaddr;
    vmaddr_t dst_vmaddr;

    CodeMemBlock *relocated;
    CodeBuffer *relocated_buffer;

    ExecuteState start_state;
    ExecuteState curr_state;
    Assembler *curr_assembler;
    ThumbTurboAssembler *thumb_assembler;
    TurboAssembler *arm_assembler;

    tinystl::unordered_map<addr_t, ExecuteState> execute_state_map;

    tinystl::unordered_map<off_t, off_t> relocated_offset_map;

    tinystl::unordered_map<vmaddr_t, AssemblerPseudoLabel *> label_map;
} relo_ctx_t;

// ---

addr_t relo_cur_src_vmaddr(relo_ctx_t *ctx) {
    int relocated_len = ctx->buffer_cursor - ctx->buffer;
    if (ctx->curr_state == zz::arm::ARMExecuteState) {
        return ctx->src_vmaddr + relocated_len + ARM_PC_OFFSET;
    } else {
        return ctx->src_vmaddr + relocated_len + Thumb_PC_OFFSET;
    }
}

static bool is_thumb2(uint32_t insn) {
    uint16_t insn1, insn2;
    insn1 = insn & 0x0000ffff;
    insn2 = (insn & 0xffff0000) >> 16;
    // refer: Top level T32 instruction set encoding
    uint32_t op0 = bits(insn1, 13, 15);
    uint32_t op1 = bits(insn1, 11, 12);

    if (op0 == 0b111 && op1 != 0b00) {
        return true;
    }
    return false;
}

bool check_execute_state_changed(relo_ctx_t *ctx, addr_t insn_addr) {
    for (auto iter = ctx->execute_state_map.begin(); iter != ctx->execute_state_map.end(); ++iter) {
        addr_t execute_state_changed_pc = iter->first;
        auto state = iter->second;
        if (execute_state_changed_pc == insn_addr) {
            return true;
        }
    }
    return false;
}

static inline int32_t SignExtend(unsigned x, int M, int N) {
#if 1
    char sign_bit = bit(x, M - 1);
    unsigned sign_mask = 0 - sign_bit;
    x |= ((sign_mask >> M) << M);
#else
    x = (long)((long)x << (N - M)) >> (N - M);
#endif
    return (int32_t) x;
}

enum arm_shift_type {
    arm_shift_lsl, arm_shift_lsr, arm_shift_asr, arm_shift_ror, arm_shift_rrx
};

uint32_t arm_shift_c(uint32_t val, uint32_t shift_type, uint32_t shift_count, uint32_t carry_in,
                     uint32_t *carry_out) {
    if (shift_count == 0)
        return val;
    uint32_t r_val;
    uint32_t carry = carry_in;
    switch (shift_type) {
        case arm_shift_lsl:
            r_val = val;
            r_val = r_val << shift_count;
            carry = (r_val >> 32) & 0x1;
            val = r_val;
            break;
        case arm_shift_lsr:
            r_val = val;
            r_val = r_val >> (shift_count - 1);
            carry = r_val & 0x1;
            val = (r_val >> 1);
            break;
        case arm_shift_asr:
            r_val = val;
            if (val & 0x80000000) {
                r_val |= 0xFFFFFFFF00000000ULL;
            }
            r_val = r_val >> (shift_count - 1);
            carry = r_val & 0x1;
            val = (r_val >> 1);
            break;
        case arm_shift_ror:
            val = (val >> (shift_count % 32)) | (val << (32 - (shift_count % 32)));
            carry = (val >> 31);
            break;
        case arm_shift_rrx:
            carry = val & 0x1;
            val = (carry_in << 31) | (val >> 1);
            break;
            break;
    }
    return val;
}

uint32_t arm_expand_imm_c(uint32_t imm12) {
    uint32_t unrotated_value = bits(imm12, 0, 7);
    return arm_shift_c(unrotated_value, arm_shift_ror, 2 * bits(imm12, 8, 11), 0, 0);
}

uint32_t A32ExpandImm(uint32_t imm12) {
    return arm_expand_imm_c(imm12);
}

static void ARMRelocateSingleInsn(relo_ctx_t *ctx, int32_t insn) {
    auto turbo_assembler_ = static_cast<TurboAssembler *>(ctx->curr_assembler);
#define _ turbo_assembler_->

    bool is_insn_relocated = false;

    // top level encoding
    uint32_t cond, op0, op1;
    cond = bits(insn, 28, 31);
    op0 = bits(insn, 25, 27);
    op1 = bit(insn, 4);
    // Load/Store Word, Unsigned byte (immediate, literal)
    if (cond != 0b1111 && op0 == 0b010) {
        uint32_t P, U, o2, W, o1, Rn, Rt, imm12;
        P = bit(insn, 24);
        U = bit(insn, 23);
        W = bit(insn, 21);
        imm12 = bits(insn, 0, 11);
        Rn = bits(insn, 16, 19);
        Rt = bits(insn, 12, 15);
        o1 = bit(insn, 20);
        o2 = bit(insn, 22);
        uint32_t P_W = (P << 1) | W;
        do {
            // LDR (literal)
            DLOG(0, "%d:relo <ldr_literal> at %p", ctx->relocated_offset_map.size(),
                 relo_cur_src_vmaddr(ctx));
            if (o1 == 1 && o2 == 0 && P_W != 0b01 && Rn == 0b1111) {
                goto load_literal_fix_scheme;
            }
            if (o1 == 1 && o2 == 1 && P_W != 0b01 && Rn == 0b1111) {
                goto load_literal_fix_scheme;
            }
            break;
            load_literal_fix_scheme:
            addr32_t dst_vmaddr = 0;
            if (U == 0b1)
                dst_vmaddr = relo_cur_src_vmaddr(ctx) + imm12;
            else
                dst_vmaddr = relo_cur_src_vmaddr(ctx) - imm12;
            Register regRt = Register::R(Rt);

            auto label = new RelocLabel(dst_vmaddr);
            _ AppendRelocLabel(label);

            if (regRt.code() == pc.code()) {
                _ Ldr(VOLATILE_REGISTER, label);
                _ ldr(regRt, MemOperand(VOLATILE_REGISTER));
            } else {
                _ Ldr(regRt, label);
                _ ldr(regRt, MemOperand(regRt));
            }

            is_insn_relocated = true;
        } while (0);
    }

    // Data-processing and miscellaneous instructions
    if (cond != 0b1111 && (op0 & 0b110) == 0b000) {
        uint32_t op0, op1, op2, op3, op4;
        op0 = bit(insn, 25);
        // Data-processing immediate
        if (op0 == 1) {
            uint32_t op0, op1;
            op0 = bits(insn, 23, 24);
            op1 = bits(insn, 20, 21);
            // Integer Data Processing (two register and immediate)
            if ((op0 & 0b10) == 0b00) {
                DLOG(0, "%d:relo <arm: adr/adrp> at %p", ctx->relocated_offset_map.size(),
                     relo_cur_src_vmaddr(ctx));

                uint32_t opc, S, Rn;
                opc = bits(insn, 21, 23);
                S = bit(insn, 20);
                Rn = bits(insn, 16, 19);

                uint32_t dst_vmaddr = -1;
                int Rd = bits(insn, 12, 15);
                int imm12 = bits(insn, 0, 11);
                uint32_t imm = arm_expand_imm_c(imm12);
                if (opc == 0b010 && S == 0b0 && Rn == 0b1111) { // ADR - A2 variant
                    dst_vmaddr = relo_cur_src_vmaddr(ctx) - imm;
                } else if (opc == 0b100 && S == 0b0 && Rn == 0b1111) { // ADR - A1 variant
                    dst_vmaddr = relo_cur_src_vmaddr(ctx) + imm;
                }

                if (dst_vmaddr != -1) {
                    Register regRd = Register::R(Rd);
                    RelocLabel *pseudoDataLabel = new RelocLabel(dst_vmaddr);
                    _ AppendRelocLabel(pseudoDataLabel);

                    _ Ldr(regRd, pseudoDataLabel);

                    is_insn_relocated = true;
                }
            }
        }
    }

    // Branch, branch with link, and block data transfer
    if ((op0 & 0b110) == 0b100) {
        uint32_t cond, op0;
        cond = bits(insn, 28, 31);
        op0 = bit(insn, 25);
        // Branch (immediate) on page F4-4034
        if (op0 == 1) {
            DLOG(0, "%d:relo <arm: b/bl/blx> at %p", ctx->relocated_offset_map.size(),
                 relo_cur_src_vmaddr(ctx));

            uint32_t H = 0, imm24 = 0;
            H = bit(insn, 24);
            imm24 = bits(insn, 0, 23);
            int32_t label = SignExtend(imm24 << 2, 2 + 24, 32);
            uint32_t dst_vmaddr = relo_cur_src_vmaddr(ctx) + label;
            bool branch_link;
            if (cond != 0b1111 && H == 0) { // B
                branch_link = false;
            } else if (cond != 0b1111 && H == 1) { // BL, BLX (immediate) - A1 on page F5-4135
                branch_link = true;
            } else if (cond == 0b1111) { // BL, BLX (immediate) - A2 on page F5-4135
                branch_link = true;
                cond = AL;
                dst_vmaddr |= 1;
            } else
                UNREACHABLE();

            if (branch_link)
                _ bl((Condition) cond, 0); // goto [dst_vmaddr]
            else
                _ b((Condition) cond, 0); // goto [dst_vmaddr]
            _ b(4);                    // goto [rest_flow]
            // [dst_vmaddr]
            _ ldr(pc, MemOperand(pc, -4));
            _ EmitAddress(dst_vmaddr);
            // [rest_flow]
            _ mov(r8, r8);

            is_insn_relocated = true;
        }
    }

    // if the insn do not needed relocate, just rewrite the origin
    if (!is_insn_relocated) {
        _ EmitARMInst(insn);
    }
}

// relocate thumb-1 instructions
static void Thumb1RelocateSingleInsn(relo_ctx_t *ctx, int16_t insn) {
    auto turbo_assembler_ = static_cast<ThumbTurboAssembler *>(ctx->curr_assembler);
#define _ turbo_assembler_->

    bool is_insn_relocated = false;

    _ AlignThumbNop();

    uint32_t op = 0, rt = 0, rm = 0, rn = 0, rd = 0, shift = 0, cond = 0;
    int32_t offset = 0;

    int32_t op0 = 0, op1 = 0;
    op0 = bits(insn, 10, 15);
    // Special data instructions and branch and exchange on page F3-3942
    if (op0 == 0b010001) {
        op0 = bits(insn, 8, 9);
        // Add, subtract, compare, move (two high registers)
        if (op0 != 0b11) {
            int rs = bits(insn, 3, 6);
            // rs is PC register
            if (rs == 15) {
                DLOG(0, "%d:relo <thumb1: add/sub/cmp/mov of pc> at %p", ctx->relocated_offset_map.size(),
                     relo_cur_src_vmaddr(ctx));

                thumb1_inst_t rewrite_inst = insn;
                set_bits(rewrite_inst, 3, 6, VOLATILE_REGISTER.code());

                auto label = new ThumbRelocLabelEntry(relo_cur_src_vmaddr(ctx), false);
                _ AppendRelocLabel(label);

                _ T2_Ldr(VOLATILE_REGISTER, label);
                _ EmitInt16(rewrite_inst);

                is_insn_relocated = true;
            }
        }

        // Branch and exchange
        if (op0 == 0b11) {
            int32_t L = bit(insn, 7);
            rm = bits(insn, 3, 6);
            // BX
            if (L == 0b0) {
                if (rm == pc.code()) {
                    DLOG(0, "%d:relo <thumb1: bx pc> at %p", ctx->relocated_offset_map.size(),
                         relo_cur_src_vmaddr(ctx));

                    vmaddr_t dst_vmaddr = relo_cur_src_vmaddr(ctx);
                    auto label = new ThumbRelocLabelEntry(dst_vmaddr, true);
                    _ AppendRelocLabel(label);

                    _ T2_Ldr(pc, label);

                    ctx->execute_state_map[dst_vmaddr] = ARMExecuteState;

                    is_insn_relocated = true;
                }
            }
            // BLX
            if (L == 0b1) {
                if (rm == pc.code()) {
                    DLOG(0, "%d:relo <thumb1: blx pc> at %p", ctx->relocated_offset_map.size(),
                         relo_cur_src_vmaddr(ctx));

                    vmaddr_t dst_vmaddr = relo_cur_src_vmaddr(ctx);
                    auto label = new ThumbRelocLabelEntry(dst_vmaddr, true);
                    _ AppendRelocLabel(label);

                    _ t2_bl(4);
                    _ t2_b(4);           // goto [rest flow]
                    _ T2_Ldr(pc, label); // goto [dst_vmaddr]
                    // [rest flow]

                    ctx->execute_state_map[dst_vmaddr] = ARMExecuteState;

                    is_insn_relocated = true;
                }
            }
        }
    }

    // LDR (literal) - T1 variant on page F5-4243
    // ldr literal
    if ((insn & 0xf800) == 0x4800) {
        DLOG(0, "%d:relo <thumb1: ldr literal> at %p", ctx->relocated_offset_map.size(),
             relo_cur_src_vmaddr(ctx));

        uint32_t imm8 = bits(insn, 0, 7);
        uint32_t imm = imm8 << 2;
        vmaddr_t dst_vmaddr = relo_cur_src_vmaddr(ctx) + imm;
        dst_vmaddr = ALIGN_FLOOR(dst_vmaddr, 4);
        rt = bits(insn, 8, 10);

        auto label = new ThumbRelocLabelEntry(dst_vmaddr, false);
        _ AppendRelocLabel(label);

        _ T2_Ldr(Register::R(rt), label);
        _ t2_ldr(Register::R(rt), MemOperand(Register::R(rt), 0));

        is_insn_relocated = true;
    }

    // Add PC/SP (immediate) on page F3-3939
    // adr
    if ((insn & 0xf800) == 0xa000) {
        DLOG(0, "%d:relo <thumb1: adr> at %p", ctx->relocated_offset_map.size(),
             relo_cur_src_vmaddr(ctx));

        rd = bits(insn, 8, 10);
        uint32_t imm8 = bits(insn, 0, 7);
        int32_t imm32 = imm8 << 2;
        vmaddr_t dst_vmaddr = relo_cur_src_vmaddr(ctx) + imm32;

        auto label = new ThumbRelocLabelEntry(dst_vmaddr, false);
        _ AppendRelocLabel(label);

        _ T2_Ldr(Register::R(rd), label);

        is_insn_relocated = true;
    }

    // Conditional branch, and Supervisor Call on page F3-3946
    // b
    if ((insn & 0xf000) == 0xd000) {
        DLOG(0, "%d:relo <thumb1: b.cond> at %p", ctx->relocated_offset_map.size(),
             relo_cur_src_vmaddr(ctx));

        uint16_t cond = bits(insn, 8, 11);
        // cond != 111x
        if (cond >= 0b1110) {
            UNREACHABLE();
        }
        uint32_t imm8 = bits(insn, 0, 7);
        int32_t imm = SignExtend(imm8 << 1, 8 + 1, 32);
        vmaddr_t dst_vmaddr = relo_cur_src_vmaddr(ctx) + imm;
        dst_vmaddr |= 1;

        auto label = new ThumbRelocLabelEntry(dst_vmaddr, true);
        _ AppendRelocLabel(label);

        thumb1_inst_t b_cond_insn = 0xe000;
        set_bits(b_cond_insn, 8, 11, cond);
        _ EmitInt16(b_cond_insn | (4 >> 1));
        _ t1_nop(); // align
        _ t2_b(4);
        _ T2_Ldr(pc, label);

        is_insn_relocated = true;
    }

    // Miscellaneous 16-bit instructions on page F3-3943
    // CBNZ, CBZ
    if ((insn & 0xf500) == 0xb100) {
        DLOG(0, "%d:relo <thumb1: cbz/cbnz> at %p", ctx->relocated_offset_map.size(),
             relo_cur_src_vmaddr(ctx));

        uint32_t imm5 = bits(insn, 3, 7);
        uint32_t i = bit(insn, 9);
        uint32_t imm = (i << 6) | (imm5 << 1);
        vmaddr_t dst_vmaddr = relo_cur_src_vmaddr(ctx) + imm;

        rn = bits(insn, 0, 2);

        auto label = new ThumbRelocLabelEntry(dst_vmaddr + 1, true);
        _ AppendRelocLabel(label);

        imm5 = bits(0x4, 1, 5);
        set_bits(insn, 3, 7, imm5);
        i = bit(0x4, 6);
        set_bit(insn, 9, i);
        _ EmitInt16(insn);
        _ t1_nop(); // align
        _ t2_b(4);  // goto [rest flow]
        _ T2_Ldr(pc, label);
        // [rest flow]

        is_insn_relocated = true;
    }

    // F3.1
    // T32 instruction set encoding
    // b
    if ((insn & 0xf800) == 0xe000) {
        DLOG(0, "%d:relo <thumb1: b> at %p", ctx->relocated_offset_map.size(),
             relo_cur_src_vmaddr(ctx));

        uint32_t imm11 = bits(insn, 0, 10);
        int32_t imm = SignExtend(imm11 << 1, 11 + 1, 32);
        vmaddr_t dst_vmaddr = relo_cur_src_vmaddr(ctx) + imm;

        auto label = new ThumbRelocLabelEntry(dst_vmaddr + 1, true);
        _ AppendRelocLabel(label);

        _ T2_Ldr(pc, label);

        is_insn_relocated = true;
    }

    // if the insn do not needed relocate, just rewrite the origin
    if (!is_insn_relocated) {
#if 0
        if (relo_cur_src_vmaddr(ctx) % Thumb2_INST_LEN)
            _ t1_nop();
#endif
        _ EmitInt16(insn);
    }
}

static void Thumb2RelocateSingleInsn(relo_ctx_t *ctx, thumb1_inst_t insn1, thumb1_inst_t insn2) {
    auto turbo_assembler_ = static_cast<ThumbTurboAssembler *>(ctx->curr_assembler);
#define _ turbo_assembler_->

    bool is_insn_relocated = false;

    // if (turbo_assembler->pc_offset() % 4) {
    //   _ t1_nop();
    // }

    _ AlignThumbNop();

    // Branches and miscellaneous control on page F3-3979
    if ((insn1 & 0xf800) == 0xf000 && (insn2 & 0x8000) == 0x8000) {
        uint32_t op1 = 0, op3 = 0;
        op1 = bits(insn1, 6, 9);
        op3 = bits(insn2, 12, 14);

        // B - T3 variant on page F5-4118
        if (((op1 & 0b1110) != 0b1110) && ((op3 & 0b101) == 0b000)) {
            DLOG(0, "%d:relo <thumb2: b.cond> at %p", ctx->relocated_offset_map.size(),
                 relo_cur_src_vmaddr(ctx));

            uint32_t S = bit(insn1, 10);
            uint32_t J1 = bit(insn2, 13);
            uint32_t J2 = bit(insn2, 11);
            uint32_t imm6 = bits(insn1, 0, 5);
            uint32_t imm11 = bits(insn2, 0, 10);

            int32_t imm =
                    SignExtend((S << 20) | (J2 << 19) | (J1 << 18) | (imm6 << 12) | (imm11 << 1),
                               1 + 1 + 1 + 6 + 11 + 1, 32);
            vmaddr_t dst_vmaddr = relo_cur_src_vmaddr(ctx) + imm;
            dst_vmaddr |= 1;

            uint32_t cond = bits(insn1, 6, 9);
            thumb1_inst_t b_cond_insn = 0xe000;
            set_bits(b_cond_insn, 8, 11, cond);
            _ EmitInt16(b_cond_insn | (4 >> 1));
            _ t1_nop(); // align
            _ t2_b(8);
            _ t2_ldr(pc, MemOperand(pc, 0));
            _ EmitAddress(dst_vmaddr);

            is_insn_relocated = true;
        }

        // B - T4 variant on page F5-4118
        if ((op3 & 0b101) == 0b001) {
            DLOG(0, "%d:relo <thumb2: b> at %p", ctx->relocated_offset_map.size(),
                 relo_cur_src_vmaddr(ctx));

            uint32_t S = bit(insn1, 10);
            uint32_t J1 = bit(insn2, 13);
            uint32_t J2 = bit(insn2, 11);
            uint32_t imm10 = bits(insn1, 0, 9);
            uint32_t imm11 = bits(insn2, 0, 10);
            uint32_t i1 = !(J1 ^ S);
            uint32_t i2 = !(J2 ^ S);

            int32_t imm =
                    SignExtend((S << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1),
                               1 + 1 + 1 + 10 + 11 + 1, 32);
            vmaddr_t dst_vmaddr = relo_cur_src_vmaddr(ctx) + imm;
            dst_vmaddr |= 1;

            _ t2_ldr(pc, MemOperand(pc, 0));
            _ EmitAddress(dst_vmaddr);

            is_insn_relocated = true;
        }

        // BL, BLX (immediate) - T1 variant on page F5-4135
        if ((op3 & 0b101) == 0b101) {
            DLOG(0, "%d:relo <thumb2: bl> at %p", ctx->relocated_offset_map.size(),
                 relo_cur_src_vmaddr(ctx));

            uint32_t S = bit(insn1, 10);
            uint32_t J1 = bit(insn2, 13);
            uint32_t J2 = bit(insn2, 11);
            uint32_t i1 = !(J1 ^ S);
            uint32_t i2 = !(J2 ^ S);
            uint32_t imm11 = bits(insn2, 0, 10);
            uint32_t imm10 = bits(insn1, 0, 9);
            int32_t imm =
                    SignExtend((S << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1),
                               1 + 1 + 1 + 10 + 11 + 1, 32);
            vmaddr_t dst_vmaddr = relo_cur_src_vmaddr(ctx) + imm;
            dst_vmaddr |= 1;

            _ t2_bl(4);
            _ t2_b(8);
            _ t2_ldr(pc, MemOperand(pc, 0));
            _ EmitAddress(dst_vmaddr);

            is_insn_relocated = true;
        }

        // BL, BLX (immediate) - T2 variant on page F5-4136
        if ((op3 & 0b101) == 0b100) {
            DLOG(0, "%d:relo <thumb2: blx> at %p", ctx->relocated_offset_map.size(),
                 relo_cur_src_vmaddr(ctx));

            uint32_t S = bit(insn1, 10);
            uint32_t J1 = bit(insn2, 13);
            uint32_t J2 = bit(insn2, 11);
            uint32_t i1 = !(J1 ^ S);
            uint32_t i2 = !(J2 ^ S);
            uint32_t imm10h = bits(insn1, 0, 9);
            uint32_t imm10l = bits(insn2, 1, 10);
            int32_t imm =
                    SignExtend((S << 24) | (i1 << 23) | (i2 << 22) | (imm10h << 12) | (imm10l << 2),
                               1 + 1 + 1 + 10 + 10 + 1, 32);
            vmaddr_t dst_vmaddr = relo_cur_src_vmaddr(ctx);
            dst_vmaddr = ALIGN_FLOOR(dst_vmaddr, 4);
            dst_vmaddr+= imm;

            _ t2_bl(4);
            _ t2_b(8);
            _ t2_ldr(pc, MemOperand(pc, 0));
            _ EmitAddress(dst_vmaddr);

            is_insn_relocated = true;
        }
    }

    // Data-processing (plain binary immediate) on page F3-3983
    if ((insn1 & (0xfa10)) == 0xf200 & (insn2 & 0x8000) == 0) {
        uint32_t op0 = 0, op1 = 0;
        op0 = bit(insn1, 8);
        op1 = bits(insn2, 5, 6);

        // Data-processing (simple immediate)
        if (op0 == 0 && (op1 & 0b10) == 0b00) {
            int o1 = bit(insn1, 7);
            int o2 = bit(insn1, 5);
            int rn = bits(insn1, 0, 3);

            // ADR
            if (((o1 == 0 && o2 == 0) || (o1 == 1 && o2 == 1)) && rn == 0b1111) {
                uint32_t i = bit(insn1, 10);
                uint32_t imm3 = bits(insn2, 12, 14);
                uint32_t imm8 = bits(insn2, 0, 7);
                uint32_t rd = bits(insn2, 8, 11);
                uint32_t imm = (i << 11) | (imm3 << 8) | imm8;

                vmaddr_t dst_vmaddr = 0;
                if (o1 == 0 && o2 == 0) { // ADR - T3 on page F5-4098
                    dst_vmaddr = relo_cur_src_vmaddr(ctx) + imm;
                } else if (o1 == 1 && o2 == 1) { // ADR - T2 on page F5-4097
                    dst_vmaddr = relo_cur_src_vmaddr(ctx) - imm;
                } else {
                    UNREACHABLE();
                }

                _ t2_ldr(Register::R(rd), MemOperand(pc, 4));
                _ t2_b(0);
                _ EmitAddress(dst_vmaddr);

                is_insn_relocated = true;
            }
        }
    }

    // Load/store single on page F3-3988
    // Load, unsigned (literal) on page F3-3992
    // Load, signed (literal) on page F3-3996
    // LDR literal (T2)
    if ((insn1 & 0xff7f) == 0xf85f) {
        uint32_t U = bit(insn1, 7);
        uint32_t imm12 = bits(insn2, 0, 11);
        uint16_t rt = bits(insn2, 12, 15);

        uint32_t imm = imm12;

        vmaddr_t dst_vmaddr = 0;
        if (U == 1) {
            dst_vmaddr = relo_cur_src_vmaddr(ctx) + imm;
        } else {
            dst_vmaddr = relo_cur_src_vmaddr(ctx) - imm;
        }

        Register regRt = Register::R(rt);

        _ t2_ldr(regRt, MemOperand(pc, 8));
        _ t2_ldr(regRt, MemOperand(regRt, 0));
        _ t2_b(4);
        _ EmitAddress(dst_vmaddr);

        is_insn_relocated = true;
    }

    // if the insn not needed relocate, just rewrite the origin
    if (!is_insn_relocated) {
#if 0
        if (relo_cur_src_vmaddr(ctx) % Thumb2_INST_LEN)
          _ t1_nop();
#endif
        _ EmitInt16(insn1);
        _ EmitInt16(insn2);
    }
}

void gen_arm_relocate_code(relo_ctx_t *ctx) {

#undef _
#define _ turbo_assembler_->
    auto turbo_assembler_ = static_cast<TurboAssembler *>(ctx->curr_assembler);
#define _ turbo_assembler_->

    auto relocated_buffer = turbo_assembler_->GetCodeBuffer();

    DLOG(0, "[arm] ARM relocate %d start >>>>>", ctx->buffer_size);

    while (ctx->buffer_cursor < ctx->buffer + ctx->buffer_size) {
        uint32_t orig_off = ctx->buffer_cursor - ctx->buffer;
        uint32_t relocated_off = relocated_buffer->GetBufferSize();
        ctx->relocated_offset_map[orig_off] = relocated_off;

        arm_inst_t insn = *(arm_inst_t *) ctx->buffer_cursor;

        int last_relo_offset = turbo_assembler_->GetCodeBuffer()->GetBufferSize();

        ARMRelocateSingleInsn(ctx, insn);
        DLOG(0, "[arm] Relocate arm insn: 0x%x", insn);

        // move to next instruction
        ctx->buffer_cursor += ARM_INST_LEN;

        // execute state changed
        addr32_t next_insn_addr = relo_cur_src_vmaddr(ctx) - ARM_PC_OFFSET;
        if (check_execute_state_changed(ctx, next_insn_addr)) {
            break;
        }
    }

    bool is_relocate_interrupted = ctx->buffer_cursor < ctx->buffer + ctx->buffer_size;
    if (is_relocate_interrupted) {
        turbo_assembler_->SetExecuteState(ThumbExecuteState);
    }
}

void gen_thumb_relocate_code(relo_ctx_t *ctx) {
    int relocated_insn_count = 0;

    auto turbo_assembler_ = static_cast<ThumbTurboAssembler *>(ctx->curr_assembler);
#define _ turbo_assembler_->

    auto relocated_buffer = turbo_assembler_->GetCodeBuffer();

    DLOG(0, "[arm] Thumb relocate %d start >>>>>", ctx->buffer_size);

    while (ctx->buffer_cursor < ctx->buffer + ctx->buffer_size) {
        uint32_t orig_off = ctx->buffer_cursor - ctx->buffer;
        uint32_t relocated_off = relocated_buffer->GetBufferSize();
        ctx->relocated_offset_map[orig_off] = relocated_off;

        // align nop
        _ t1_nop();

        thumb2_inst_t insn = *(thumb2_inst_t *) ctx->buffer_cursor;

        int last_relo_offset = relocated_buffer->GetBufferSize();
        if (is_thumb2(insn)) {
            Thumb2RelocateSingleInsn(ctx, (uint16_t) insn, (uint16_t) (insn >> 16));
            DLOG(0, "[arm] Relocate thumb2 insn: 0x%x", insn);
        } else {
            Thumb1RelocateSingleInsn(ctx, (uint16_t) insn);
            DLOG(0, "[arm] Relocate thumb1 insn: 0x%x", (uint16_t) insn);
        }

        // Move to next instruction
        if (is_thumb2(insn)) {
            ctx->buffer_cursor += Thumb2_INST_LEN;
        } else {
            ctx->buffer_cursor += Thumb1_INST_LEN;
        }

        // execute state changed
        addr32_t next_insn_addr = relo_cur_src_vmaddr(ctx) - Thumb_PC_OFFSET;
        if (check_execute_state_changed(ctx, next_insn_addr)) {
            break;
        }
    }

    //  .thumb1 bx pc
    //  .thumb1 mov r8, r8
    //  .arm ldr pc, [pc, #-4]

    bool is_relocate_interrupted = ctx->buffer_cursor < ctx->buffer + ctx->buffer_size;
    if (is_relocate_interrupted) {
        turbo_assembler_->SetExecuteState(ARMExecuteState);
    }
}

void GenRelocateCode(void *buffer, CodeMemBlock *origin, CodeMemBlock *relocated, bool branch) {
    relo_ctx_t ctx;

    if ((addr_t) buffer % 2) {
        ctx.start_state = ThumbExecuteState;
        ctx.curr_state = ThumbExecuteState;
        // remove thumb address flag
        buffer = (void *) ((addr_t) buffer - 1);
    } else {
        ctx.start_state = ARMExecuteState;
        ctx.curr_state = ARMExecuteState;
    }

    ctx.buffer = ctx.buffer_cursor = (uint8_t *) buffer;
    ctx.buffer_size = origin->size;

    ctx.src_vmaddr = (vmaddr_t) origin->addr;
    ctx.dst_vmaddr = 0;

    auto *relocated_buffer = new CodeBuffer();
    ctx.relocated_buffer = relocated_buffer;

    ThumbTurboAssembler thumb_turbo_assembler_(0, ctx.relocated_buffer);
#define thumb_ thumb_turbo_assembler_.
    TurboAssembler arm_turbo_assembler_(0, ctx.relocated_buffer);
#define arm_ arm_turbo_assembler_.

    if (ctx.start_state == ThumbExecuteState)
        ctx.curr_assembler = &thumb_turbo_assembler_;
    else
        ctx.curr_assembler = &arm_turbo_assembler_;

    relocate_remain:
    if (ctx.curr_state == ThumbExecuteState) {
        ctx.curr_assembler = &thumb_turbo_assembler_;
        gen_thumb_relocate_code(&ctx);
        if (thumb_turbo_assembler_.GetExecuteState() == ARMExecuteState) {
            // translate interrupt as execute state changed
            bool is_translate_interrupted = ctx.buffer_cursor < ctx.buffer + ctx.buffer_size;
            if (is_translate_interrupted) {
                // add nop to align ARM
                if (thumb_turbo_assembler_.pc_offset() % 4)
                    thumb_turbo_assembler_.t1_nop();
                goto relocate_remain;
            }
        }
    } else {
        ctx.curr_assembler = &arm_turbo_assembler_;
        gen_arm_relocate_code(&ctx);
        if (arm_turbo_assembler_.GetExecuteState() == ThumbExecuteState) {
            bool is_translate_interrupted = ctx.buffer_cursor < ctx.buffer + ctx.buffer_size;
            // translate interrupt as execute state changed
            if (is_translate_interrupted) {
                goto relocate_remain;
            }
        }
    }

    // update origin
    int new_origin_len = (addr_t)ctx.buffer_cursor - (addr_t)ctx.buffer;
    origin->reset(origin->addr, new_origin_len);

    // TODO: if last insn is unlink branch, skip
    if (branch) {
        if (ctx.curr_state == ThumbExecuteState) {
            // branch to the rest of instructions
            thumb_ AlignThumbNop();
            thumb_ t2_ldr(pc, MemOperand(pc, 0));
            // get the real branch address
            thumb_ EmitAddress(origin->addr + origin->size + THUMB_ADDRESS_FLAG);
        } else {
            // branch to the rest of instructions
            CodeGen codegen(&arm_turbo_assembler_);
            // get the real branch address
            codegen.LiteralLdrBranch(origin->addr + origin->size);
        }
    }

    // fixup the insn branch into trampoline(has been modified)
    arm_turbo_assembler_.RelocLabelFixup(&ctx.relocated_offset_map);
    thumb_turbo_assembler_.RelocLabelFixup(&ctx.relocated_offset_map);

    // realize all the pseudo data label
    thumb_turbo_assembler_.RelocBind();
    arm_turbo_assembler_.RelocBind();

    // generate executable code
    {
        // assembler without specific memory address
        auto relocated_mem = MemoryAllocator::SharedAllocator()->allocateExecMemory(
                relocated_buffer->GetBufferSize());
        if (relocated_mem == nullptr)
            return;

        thumb_turbo_assembler_.SetRealizedAddress((void *) relocated_mem);
        arm_turbo_assembler_.SetRealizedAddress((void *) relocated_mem);

        AssemblyCode *code = NULL;
        code = AssemblyCodeBuilder::FinalizeFromTurboAssembler(ctx.curr_assembler);
        relocated->reset(code->addr, code->size);
    }

    // thumb
    if (ctx.start_state == ThumbExecuteState) {
        // add thumb address flag
        relocated->reset(relocated->addr + THUMB_ADDRESS_FLAG, relocated->size);
    }

    // clean
    {
        thumb_turbo_assembler_.ClearCodeBuffer();
        arm_turbo_assembler_.ClearCodeBuffer();

        delete relocated_buffer;
    }
}

void GenRelocateCodeAndBranch(void *buffer, CodeMemBlock *origin, CodeMemBlock *relocated) {
    GenRelocateCode(buffer, origin, relocated, true);
}

#endif
