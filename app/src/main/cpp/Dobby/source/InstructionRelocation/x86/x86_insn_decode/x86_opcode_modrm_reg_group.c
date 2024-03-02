/* Escape groups are indexed by modrm reg field. */
static x86_insn_group8_t x86_insn_modrm_reg_groups[] = {
    [X86_INSN_MODRM_REG_GROUP_1].insns =
        {
            op0(add),
            op0(or),
            op0(adc),
            op0(sbb),
            op0(and),
            op0(sub),
            op0(xor),
            op0(cmp),
        },

    [X86_INSN_MODRM_REG_GROUP_1a].insns =
        {
            op0f(pop, X86_INSN_SPEC_DEFAULT_64_BIT),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_MODRM_REG_GROUP_2].insns =
        {
            op0(rol),
            op0(ror),
            op0(rcl),
            op0(rcr),
            op0(shl),
            op0(shr),
            op0(sal),
            op0(sar),
        },

    [X86_INSN_MODRM_REG_GROUP_3].insns =
        {
            op0(test),
            op0(test),
            op0(not ),
            op0(neg),
            op0(mul),
            op0(imul),
            op0(div),
            op0(idiv),
        },

    [X86_INSN_MODRM_REG_GROUP_4].insns =
        {
            op0(inc),
            op0(dec),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_MODRM_REG_GROUP_5].insns =
        {
            op1(inc, Ev),
            op1(dec, Ev),
            op1f(call, X86_INSN_SPEC_DEFAULT_64_BIT, Ev),
            op1(call, Mp),
            op1f(jmp, X86_INSN_SPEC_DEFAULT_64_BIT, Ev),
            op1(jmp, Mp),
            op1f(push, X86_INSN_SPEC_DEFAULT_64_BIT, Ev),
            op0(bad),
        },

    [X86_INSN_MODRM_REG_GROUP_6].insns =
        {
            op1(sldt, Ev),
            op1(str, Ev),
            op1(lldt, Ev),
            op1(ltr, Ev),
            op1(verr, Ev),
            op1(verw, Ev),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_MODRM_REG_GROUP_7].insns =
        {
            op1(sgdt, Mv),
            op1(sidt, Mv),
            op1(lgdt, Mv),
            op1(lidt, Mv),
            op1(smsw, Ev),
            op0(bad),
            op1(lmsw, Ew),
            op1(invlpg, Mv),
        },

    [X86_INSN_MODRM_REG_GROUP_8].insns =
        {
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op2(bt, Ev, Ib),
            op2(bts, Ev, Ib),
            op2(btr, Ev, Ib),
            op2(btc, Ev, Ib),
        },

    [X86_INSN_MODRM_REG_GROUP_9].insns =
        {
            op0(bad),
            op1(cmpxchg, Mx),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_MODRM_REG_GROUP_10].insns =
        {
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_MODRM_REG_GROUP_11].insns =
        {
            op0(mov),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_MODRM_REG_GROUP_12].insns =
        {
            op0(bad),
            op0(bad),
            op2(psrlw, Rm, Ib),
            op0(bad),
            op2(psraw, Rm, Ib),
            op0(bad),
            op2(psllw, Rm, Ib),
            op0(bad),
        },

    [X86_INSN_MODRM_REG_GROUP_13].insns =
        {
            op0(bad),
            op0(bad),
            op2(psrld, Rm, Ib),
            op0(bad),
            op2(psrad, Rm, Ib),
            op0(bad),
            op2(pslld, Rm, Ib),
            op0(bad),
        },

    [X86_INSN_MODRM_REG_GROUP_14].insns =
        {
            op0(bad),
            op0(bad),
            op2(psrlq, Rm, Ib),
            op0f(bad, 0),
            op0(bad),
            op0(bad),
            op2(psllq, Rm, Ib),
            op0f(bad, 0),
        },

    [X86_INSN_MODRM_REG_GROUP_15].insns =
        {
            op1(fxsave, Mv),
            op1(fxrstor, Mv),
            op1(ldmxcsr, Mv),
            op1(stmxcsr, Mv),
            op0(bad),
            op1(lfence, Mv),
            op1(mfence, Mv),
            op1(sfence, Mv),
        },

    [X86_INSN_MODRM_REG_GROUP_16].insns =
        {
            op1(prefetch_nta, Mv),
            op1(prefetch_t0, Mv),
            op1(prefetch_t1, Mv),
            op1(prefetch_t2, Mv),
            op1(prefetch_nop, Mv),
            op1(prefetch_nop, Mv),
            op1(prefetch_nop, Mv),
            op1(prefetch_nop, Mv),
        },

    [X86_INSN_MODRM_REG_GROUP_p].insns =
        {
            op1(prefetch_exclusive, Mv),
            op1(prefetch_modified, Mv),
            op1(prefetch_nop, Mv),
            op1(prefetch_modified, Mv),
            op1(prefetch_nop, Mv),
            op1(prefetch_nop, Mv),
            op1(prefetch_nop, Mv),
            op1(prefetch_nop, Mv),
        },
};
