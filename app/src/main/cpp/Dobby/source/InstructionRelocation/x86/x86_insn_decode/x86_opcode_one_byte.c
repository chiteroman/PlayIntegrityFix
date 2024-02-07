// clang-format off
static x86_insn_spec_t x86_opcode_map_one_byte[256] = {
    /* 0x00 */
    foreach_x86_operand_combine(add, op_dst, op_src),
    op0(push_es),
    op0f(pop_es, X86_INSN_SPEC_DEFAULT_64_BIT),
    foreach_x86_operand_combine(or, op_dst, op_src),
    op0f(push_cs, X86_INSN_SPEC_DEFAULT_64_BIT),
    op0(escape_two_byte),

    /* 0x10 */
    foreach_x86_operand_combine(adc, op_dst, op_src),
    op0(push_ss),
    op0(pop_ss),
    foreach_x86_operand_combine(sbb, op_dst, op_src),
    op0(push_ds),
    op0(pop_ds),

    /* 0x20 */
    foreach_x86_operand_combine(and, op_dst, op_src),
    op0(segment_es),
    op0(daa),
    foreach_x86_operand_combine(sub, op_dst, op_src),
    op0(segment_cs),
    op0(das),

    /* 0x30 */
    foreach_x86_operand_combine(xor, op_dst, op_src),
    op0(segment_ss),
    op0(aaa),
    foreach_x86_operand_combine(cmp, op_src, op_src),
    op0(segment_ds),
    op0(aas),

/* 0x40 */
#define _(r) op1(inc, r),
    foreach_x86_gp_reg
#undef _
#define _(r) op1(dec, r),
    foreach_x86_gp_reg
#undef _

/* 0x50 */
#define _(r) op1f(push, X86_INSN_SPEC_DEFAULT_64_BIT, r),
    foreach_x86_gp_reg
#undef _
#define _(r) op1f(pop, X86_INSN_SPEC_DEFAULT_64_BIT, r),
    foreach_x86_gp_reg
#undef _

    /* 0x60 */
    op0(pusha),
    op0(popa),
    op2(bound, Gv, Ma),
    op2(movsxd, Gv, Ed),
    op0(segment_fs),
    op0(segment_gs),
    op0(operand_type),
    op0(address_size),
    op1f(push, X86_INSN_SPEC_DEFAULT_64_BIT, Iz),
    op3(imul, Gv, Ev, Iz),
    op1f(push, X86_INSN_SPEC_DEFAULT_64_BIT, Ib),
    op3(imul, Gv, Ev, Ib),
    op1(insb, DX),
    op1(insw, DX),
    op1(outsb, DX),
    op1(outsw, DX),

/* 0x70 */
#define _(x) op1(j##x, Jb),
    foreach_x86_condition
#undef _

    /* 0x80 */
    op2f(modrm_group_1, X86_INSN_FLAG_MODRM_REG_GROUP_1, Eb, Ib),
    op2f(modrm_group_1, X86_INSN_FLAG_MODRM_REG_GROUP_1, Ev, Iz),
    op2f(modrm_group_1, X86_INSN_FLAG_MODRM_REG_GROUP_1, Eb, Ib),
    op2f(modrm_group_1, X86_INSN_FLAG_MODRM_REG_GROUP_1, Ev, Ib),
    op2(test, Eb, Gb),
    op2(test, Ev, Gv),
    op2(xchg, Eb, Gb),
    op2(xchg, Ev, Gv),
    op2(mov, Eb, Gb),
    op2(mov, Ev, Gv),
    op2(mov, Gb, Eb),
    op2(mov, Gv, Ev),
    op2(mov, Ev, Sw),
    op2(lea, Gv, Ev),
    op2(mov, Sw, Ew),
    op1f(modrm_group_1a, X86_INSN_FLAG_MODRM_REG_GROUP_1a, Ev),

    /* 0x90 */
    op0(nop),
    op1(xchg, CX),
    op1(xchg, DX),
    op1(xchg, BX),
    op1(xchg, SP),
    op1(xchg, BP),
    op1(xchg, SI),
    op1(xchg, DI),
    op0(cbw),
    op0(cwd),
    op1(call, Ap),
    op0(wait),
    op0(pushf),
    op0(popf),
    op0(sahf),
    op0(lahf),

    /* 0xa0 */
    op2(mov, AL, Ob),
    op2(mov, AX, Ov),
    op2(mov, Ob, AL),
    op2(mov, Ov, AX),
    op0(movsb),
    op0(movsw),
    op0(cmpsb),
    op0(cmpsw),
    op2(test, AL, Ib),
    op2(test, AX, Iz),
    op1(stosb, AL),
    op1(stosw, AX),
    op1(lodsb, AL),
    op1(lodsw, AX),
    op1(scasb, AL),
    op1(scasw, AX),

    /* 0xb0 */
    op2(mov, AL, Ib),
    op2(mov, CL, Ib),
    op2(mov, DL, Ib),
    op2(mov, BL, Ib),
    op2(mov, AH, Ib),
    op2(mov, CH, Ib),
    op2(mov, DH, Ib),
    op2(mov, BH, Ib),
#define _(r) op2(mov, r, Iv),
    foreach_x86_gp_reg
#undef _

    /* 0xc0 */
    op2f(modrm_group_2, X86_INSN_FLAG_MODRM_REG_GROUP_2, Eb, Ib),
    op2f(modrm_group_2, X86_INSN_FLAG_MODRM_REG_GROUP_2, Ev, Ib),
    op1(ret, Iw),
    op0(ret),
    op2(les, Gz, Mp),
    op2(lds, Gz, Mp),
    op2f(modrm_group_11, X86_INSN_FLAG_MODRM_REG_GROUP_11, Eb, Ib),
    op2f(modrm_group_11, X86_INSN_FLAG_MODRM_REG_GROUP_11, Ev, Iz),
    op2(enter, Iw, Ib),
    op0(leave),
    op1(ret, Iw),
    op0(ret),
    op0(int3),
    op1(int, Ib),
    op0(into),
    op0(iret),

    /* 0xd0 */
    op2f(modrm_group_2, X86_INSN_FLAG_MODRM_REG_GROUP_2, Eb, 1b),
    op2f(modrm_group_2, X86_INSN_FLAG_MODRM_REG_GROUP_2, Ev, 1b),
    op2f(modrm_group_2, X86_INSN_FLAG_MODRM_REG_GROUP_2, Eb, CL),
    op2f(modrm_group_2, X86_INSN_FLAG_MODRM_REG_GROUP_2, Ev, CL),
    op0(aam),
    op0(aad),
    op0(salc),
    op0(xlat),
    /* FIXME x87 */
    op0(bad),
    op0(bad),
    op0(bad),
    op0(bad),
    op0(bad),
    op0(bad),
    op0(bad),
    op0(bad),

    /* 0xe0 */
    op1(loopnz, Jb),
    op1(loopz, Jb),
    op1(loop, Jb),
    op1(jcxz, Jb),
    op2(in, AL, Ib),
    op2(in, AX, Ib),
    op2(out, Ib, AL),
    op2(out, Ib, AX),
    op1f(call, X86_INSN_SPEC_DEFAULT_64_BIT, Jz),
    op1f(jmp, X86_INSN_SPEC_DEFAULT_64_BIT, Jz),
    op1(jmp, Ap),
    op1(jmp, Jb),
    op2(in, AL, DX),
    op2(in, AX, DX),
    op2(out, DX, AL),
    op2(out, DX, AX),

    /* 0xf0 */
    op0(lock),
    op0(int1),
    op0(repne),
    op0(rep),
    op0(hlt),
    op0(cmc),
    op0f(modrm_group_3, X86_INSN_FLAG_MODRM_REG_GROUP_3),
    op0f(modrm_group_3, X86_INSN_FLAG_MODRM_REG_GROUP_3),
    op0(clc),
    op0(stc),
    op0(cli),
    op0(sti),
    op0(cld),
    op0(std),
    op1f(modrm_group_4, X86_INSN_FLAG_MODRM_REG_GROUP_4, Eb),
    op0f(modrm_group_5, X86_INSN_FLAG_MODRM_REG_GROUP_5),
};

// clang-format on