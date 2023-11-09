static x86_insn_group8_t x86_insn_sse_groups_repz[] = {
    [X86_INSN_SSE_GROUP_10].insns =
        {
            op2(movss, Gx, Ex),
            op2(movss, Ex, Gx),
            op2(movsldup, Gx, Ex),
            op0(bad),
            op0(bad),
            op0(bad),
            op2(movshdup, Gx, Ex),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_28].insns =
        {
            op0(bad),
            op0(bad),
            op2(cvtsi2ss, Gx, Ev),
            op0(bad),
            op2(cvttss2si, Gv, Ex),
            op2(cvtss2si, Gv, Ex),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_50].insns =
        {
            op0(bad),
            op2(sqrtss, Gx, Ex),
            op2(rsqrtps, Gx, Ex),
            op2(rcpss, Gx, Ex),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_58].insns =
        {
            op2(addss, Gx, Ex),
            op2(mulss, Gx, Ex),
            op2(cvtss2sd, Gx, Ex),
            op2(cvttps2dq, Gx, Ex),
            op2(subss, Gx, Ex),
            op2(minss, Gx, Ex),
            op2(divss, Gx, Ex),
            op2(maxss, Gx, Ex),
        },

    [X86_INSN_SSE_GROUP_60].insns =
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

    [X86_INSN_SSE_GROUP_68].insns =
        {
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op2(movdqu, Gx, Ex),
        },

    [X86_INSN_SSE_GROUP_70].insns =
        {
            op3(pshufhw, Gx, Ex, Ib),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_78].insns =
        {
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op2(movq, Gx, Ex),
            op2(movdqu, Ex, Gx),
        },

    [X86_INSN_SSE_GROUP_c0].insns =
        {
            op0(bad),
            op0(bad),
            op3(cmpss, Gx, Ex, Ib),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_d0].insns =
        {
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op2(movq2dq, Gx, Em),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_d8].insns =
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

    [X86_INSN_SSE_GROUP_e0].insns =
        {
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op2(cvtdq2pd, Gx, Ex),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_e8].insns =
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

    [X86_INSN_SSE_GROUP_f0].insns =
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

    [X86_INSN_SSE_GROUP_f8].insns =
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
};

static x86_insn_group8_t x86_insn_sse_groups_operand_size[] = {
    [X86_INSN_SSE_GROUP_10].insns =
        {
            op2(movupd, Gx, Ex),
            op2(movupd, Ex, Gx),
            op2(movlpd, Gx, Ex),
            op2(movlpd, Ex, Gx),
            op2(unpcklpd, Gx, Ex),
            op2(unpckhpd, Gx, Ex),
            op2(movhpd, Gx, Mx),
            op2(movhpd, Mx, Gx),
        },

    [X86_INSN_SSE_GROUP_28].insns =
        {
            op2(movapd, Gx, Ex),
            op2(movapd, Ex, Gx),
            op2(cvtpi2pd, Gx, Ex),
            op2(movntpd, Mx, Gx),
            op2(cvttpd2pi, Gx, Mx),
            op2(cvtpd2pi, Gx, Mx),
            op2(ucomisd, Gx, Ex),
            op2(comisd, Gx, Ex),
        },

    [X86_INSN_SSE_GROUP_50].insns =
        {
            op2(movmskpd, Gd, Rx),
            op2(sqrtpd, Gx, Ex),
            op0(bad),
            op0(bad),
            op2(andpd, Gx, Ex),
            op2(andnpd, Gx, Ex),
            op2(orpd, Gx, Ex),
            op2(xorpd, Gx, Ex),
        },

    [X86_INSN_SSE_GROUP_58].insns =
        {
            op2(addpd, Gx, Ex),
            op2(mulpd, Gx, Ex),
            op2(cvtpd2ps, Gx, Ex),
            op2(cvtps2dq, Gx, Ex),
            op2(subpd, Gx, Ex),
            op2(minpd, Gx, Ex),
            op2(divpd, Gx, Ex),
            op2(maxpd, Gx, Ex),
        },

    [X86_INSN_SSE_GROUP_60].insns =
        {
            op2(punpcklbw, Gx, Ex),
            op2(punpcklwd, Gx, Ex),
            op2(punpckldq, Gx, Ex),
            op2(packsswb, Gx, Ex),
            op2(pcmpgtb, Gx, Ex),
            op2(pcmpgtw, Gx, Ex),
            op2(pcmpgtd, Gx, Ex),
            op2(packuswb, Gx, Ex),
        },

    [X86_INSN_SSE_GROUP_68].insns =
        {
            op2(punpckhbw, Gx, Ex),
            op2(punpckhwd, Gx, Ex),
            op2(punpckhdq, Gx, Ex),
            op2(packssdw, Gx, Ex),
            op2(punpcklqdq, Gx, Ex),
            op2(punpckhqdq, Gx, Ex),
            op2(movd, Gx, Ev),
            op2(movdqa, Gx, Ex),
        },

    [X86_INSN_SSE_GROUP_70].insns =
        {
            op3(pshufd, Gx, Ex, Ib),
            op0f(modrm_group_12, X86_INSN_FLAG_MODRM_REG_GROUP_12),
            op0f(modrm_group_13, X86_INSN_FLAG_MODRM_REG_GROUP_13),
            op0f(modrm_group_14, X86_INSN_FLAG_MODRM_REG_GROUP_14),
            op2(pcmpeqb, Gx, Ex),
            op2(pcmpeqw, Gx, Ex),
            op2(pcmpeqd, Gx, Ex),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_78].insns =
        {
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op2(haddpd, Gx, Ex),
            op2(hsubpd, Gx, Ex),
            op2(movd, Ev, Gx),
            op2(movdqa, Ex, Gx),
        },

    [X86_INSN_SSE_GROUP_c0].insns =
        {
            op0(bad),
            op0(bad),
            op3(cmppd, Gx, Ex, Ib),
            op0(bad),
            op3(pinsrw, Gx, Ew, Ib),
            op3(pextrw, Gd, Gx, Ib),
            op3(shufpd, Gx, Ex, Ib),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_d0].insns =
        {
            op2(addsubpd, Gx, Ex),
            op2(psrlw, Gx, Ex),
            op2(psrld, Gx, Ex),
            op2(psrlq, Gx, Ex),
            op2(paddq, Gx, Ex),
            op2(pmullw, Gx, Ex),
            op2(movq, Ex, Gx),
            op2(pmovmskb, Gd, Rx),
        },

    [X86_INSN_SSE_GROUP_d8].insns =
        {
            op2(psubusb, Gx, Ex),
            op2(psubusw, Gx, Ex),
            op2(pminub, Gx, Ex),
            op2(pand, Gx, Ex),
            op2(paddusb, Gx, Ex),
            op2(paddusw, Gx, Ex),
            op2(pmaxub, Gx, Ex),
            op2(pandn, Gx, Ex),
        },

    [X86_INSN_SSE_GROUP_e0].insns =
        {
            op2(pavgb, Gx, Ex),
            op2(psraw, Gx, Ex),
            op2(psrad, Gx, Ex),
            op2(pavgw, Gx, Ex),
            op2(pmulhuw, Gx, Ex),
            op2(pmulhw, Gx, Ex),
            op2(cvttpd2dq, Gx, Ex),
            op2(movntdq, Mx, Gx),
        },

    [X86_INSN_SSE_GROUP_e8].insns =
        {
            op2(psubsb, Gx, Ex),
            op2(psubsw, Gx, Ex),
            op2(pminsw, Gx, Ex),
            op2(por, Gx, Ex),
            op2(paddsb, Gx, Ex),
            op2(paddsw, Gx, Ex),
            op2(pmaxsw, Gx, Ex),
            op2(pxor, Gx, Ex),
        },

    [X86_INSN_SSE_GROUP_f0].insns =
        {
            op0(bad),
            op2(psllw, Gx, Ex),
            op2(pslld, Gx, Ex),
            op2(psllq, Gx, Ex),
            op2(pmuludq, Gx, Ex),
            op2(pmaddwd, Gx, Ex),
            op2(psadbw, Gx, Ex),
            op2(maskmovdqu, Gx, Ex),
        },

    [X86_INSN_SSE_GROUP_f8].insns =
        {
            op2(psubb, Gx, Ex),
            op2(psubw, Gx, Ex),
            op2(psubd, Gx, Ex),
            op2(psubq, Gx, Ex),
            op2(paddb, Gx, Ex),
            op2(paddw, Gx, Ex),
            op2(paddd, Gx, Ex),
            op0(bad),
        },
};

static x86_insn_group8_t x86_insn_sse_groups_repnz[] = {
    [X86_INSN_SSE_GROUP_10].insns =
        {
            op2(movsd, Gx, Ex),
            op2(movsd, Ex, Gx),
            op2(movddup, Gx, Ex),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_28].insns =
        {
            op0(bad),
            op0(bad),
            op2(cvtsi2sd, Gx, Ev),
            op0(bad),
            op2(cvttsd2si, Gv, Ex),
            op2(cvtsd2si, Gv, Ex),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_50].insns =
        {
            op0(bad),
            op2(sqrtsd, Gx, Ex),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_58].insns =
        {
            op2(addsd, Gx, Ex),
            op2(mulsd, Gx, Ex),
            op2(cvtsd2ss, Gx, Ex),
            op0(bad),
            op2(subsd, Gx, Ex),
            op2(minsd, Gx, Ex),
            op2(divsd, Gx, Ex),
            op2(maxsd, Gx, Ex),
        },

    [X86_INSN_SSE_GROUP_60].insns =
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

    [X86_INSN_SSE_GROUP_68].insns =
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

    [X86_INSN_SSE_GROUP_70].insns =
        {
            op3(pshuflw, Gx, Ex, Ib),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_78].insns =
        {
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op2(haddps, Gx, Ex),
            op2(hsubps, Gx, Ex),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_c0].insns =
        {
            op0(bad),
            op0(bad),
            op3(cmpsd, Gx, Ex, Ib),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_d0].insns =
        {
            op2(addsubps, Gx, Ex),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op2(movdq2q, Gm, Ex),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_d8].insns =
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

    [X86_INSN_SSE_GROUP_e0].insns =
        {
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op2(cvtpd2dq, Gx, Ex),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_e8].insns =
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

    [X86_INSN_SSE_GROUP_f0].insns =
        {
            op2(lddqu, Gx, Mx),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
            op0(bad),
        },

    [X86_INSN_SSE_GROUP_f8].insns =
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
};