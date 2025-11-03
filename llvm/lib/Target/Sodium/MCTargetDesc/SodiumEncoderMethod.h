//Default EncoderMethod.
unsigned getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                           SmallVectorImpl<MCFixup> &Fixups,
                           const MCSubtargetInfo &STI) const {
  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());
  //llvm_unreachable("Unhandled expression!");
  //return 0;
  return getExprOpValue(MI, MO.getExpr(), Fixups, STI);
}

unsigned getImmOpValue(const MCInst &MI, unsigned OpNo,
                       SmallVectorImpl<MCFixup> &Fixups,
                       const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  // If the destination is an immediate, there is nothing to do.
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());
  assert(MO.isExpr() &&
         "getImmOpValue expects only expressions or immediates");
  return getExprOpValue(MI, MO.getExpr(), Fixups, STI);
}

unsigned getImmOpValueSub1(const MCInst &MI, unsigned OpNo,
                           SmallVectorImpl<MCFixup> &Fixups,
                           const MCSubtargetInfo &STI) const {
  return MI.getOperand(OpNo).getImm() - 1;
}

template <unsigned N>
unsigned getImmOpValueAsr(const MCInst &MI, unsigned OpNo,
                          SmallVectorImpl<MCFixup> &Fixups,
                          const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm()) {
      unsigned Res = MO.getImm();
      assert((Res & ((1U << N) - 1U)) == 0 && "lowest N bits are non-zero");
      return Res >> N;
  }
  return getImmOpValue(MI, OpNo, Fixups, STI);
}
