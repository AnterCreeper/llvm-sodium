bool SelectBaseAddr(SDValue Addr, SDValue &Base) {
  // If this is FrameIndex, select it directly. Otherwise just let it get
  // selected to a register independently.
  if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr))
    Base =
        CurDAG->getTargetFrameIndex(FIN->getIndex(), Addr.getValueType());
  else
    Base = Addr;
  return true;
}

// Fold constant addresses.
bool SelectAddrConstant(SDValue Addr, SDValue &Base,
                                            SDValue &Offset) {
  SDLoc DL(Addr);
  MVT VT = Addr.getSimpleValueType();

  if (!isa<ConstantSDNode>(Addr))
    return false;

  // If the constant is a simm13, we can fold the whole constant and use X0 as
  // the base.
  int64_t CVal = cast<ConstantSDNode>(Addr)->getSExtValue();
  if (!isInt<13>(CVal))
    return false;
  Base = CurDAG->getRegister(Sodium::X0, VT);
  Offset = CurDAG->getTargetConstant(SignExtend32<13>(CVal), DL, VT);
  return true;
}

bool SelectSExtBits(SDValue N, unsigned Bits, SDValue &Val) {
  if (N.getOpcode() == ISD::SIGN_EXTEND_INREG &&
      cast<VTSDNode>(N.getOperand(1))->getVT().getSizeInBits() == Bits) {
    Val = N.getOperand(0);
    return true;
  }
  auto UnwrapShlSra = [](SDValue N, unsigned ShiftAmt) {
    if (N.getOpcode() != ISD::SRA || !isa<ConstantSDNode>(N.getOperand(1)))
      return N;
    SDValue N0 = N.getOperand(0);
    if (N0.getOpcode() == ISD::SHL && isa<ConstantSDNode>(N0.getOperand(1)) &&
        N.getConstantOperandVal(1) == ShiftAmt &&
        N0.getConstantOperandVal(1) == ShiftAmt)
      return N0.getOperand(0);
    return N;
  };
  MVT VT = N.getSimpleValueType();
  if (CurDAG->ComputeNumSignBits(N) > (VT.getSizeInBits() - Bits)) {
    Val = UnwrapShlSra(N, VT.getSizeInBits() - Bits);
    return true;
  }
  return false;
}

bool SelectZExtBits(SDValue N, unsigned Bits, SDValue &Val) {
  if (N.getOpcode() == ISD::AND) {
    auto *C = dyn_cast<ConstantSDNode>(N.getOperand(1));
    if (C && C->getZExtValue() == maskTrailingOnes<uint64_t>(Bits)) {
      Val = N.getOperand(0);
      return true;
    }
  }
  MVT VT = N.getSimpleValueType();
  APInt Mask = APInt::getBitsSetFrom(VT.getSizeInBits(), Bits);
  if (CurDAG->MaskedValueIsZero(N, Mask)) {
    Val = N;
    return true;
  }
  return false;
}

template <unsigned Bits> bool SelectSExtBits(SDValue N, SDValue &Val) {
    return SelectSExtBits(N, Bits, Val);
}
template <unsigned Bits> bool SelectZExtBits(SDValue N, SDValue &Val) {
    return SelectZExtBits(N, Bits, Val);
}
