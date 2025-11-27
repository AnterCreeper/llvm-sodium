static SDValue getTargetNode(GlobalAddressSDNode *N, const SDLoc &DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetGlobalAddress(N->getGlobal(), DL, Ty, 0, Flags);
}

static SDValue getTargetNode(BlockAddressSDNode *N, const SDLoc &DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetBlockAddress(N->getBlockAddress(), Ty, N->getOffset(),
                                   Flags);
}

static SDValue getTargetNode(ConstantPoolSDNode *N, const SDLoc &DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetConstantPool(N->getConstVal(), Ty, N->getAlign(),
                                   N->getOffset(), Flags);
}

static SDValue getTargetNode(JumpTableSDNode *N, const SDLoc &DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetJumpTable(N->getIndex(), Ty, Flags);
}

template <class NodeTy>
SDValue SodiumTargetLowering::getAddr(NodeTy *N, SelectionDAG &DAG,
                                      bool IsLocal) const {
  SDLoc DL(N);
  EVT Ty = getPointerTy(DAG.getDataLayout());
  SDValue Addr = getTargetNode(N, DL, Ty, DAG, 0);

  if (isPositionIndependent()) {
    // This generates the pattern (PseudoLA sym), which expands to
    //   addi (auipc %pcrel_hi(sym)), %pcrel_lo(sym).
    return DAG.getNode(SodiumISD::LA, DL, Ty, Addr);
  }

  //NonPIC Address
  SDValue AddrHi = getTargetNode(N, DL, Ty, DAG, SODIUMII::MO_HI16);
  SDValue AddrLo = getTargetNode(N, DL, Ty, DAG, SODIUMII::MO_LO16);

  bool is32Bit = Subtarget.is32Bit;
  if (is32Bit) {
    return DAG.getNode(ISD::BUILD_VECTOR, DL, MVT::v2i16,
                       DAG.getNode(SodiumISD::LI, DL, Ty, AddrLo),
                       DAG.getNode(SodiumISD::LI, DL, Ty, AddrHi));
  } else {
    return AddrLo;
  }
}

template<typename T>
SDValue SodiumTargetLowering::LowerSimpleAddress(SDValue Op,
                                                 SelectionDAG &DAG) const {
  T *N = cast<T>(Op);
  return getAddr(N, DAG);
}

SDValue SodiumTargetLowering::LowerGlobalAddress(SDValue Op,
                                                 SelectionDAG &DAG) const {
  GlobalAddressSDNode *N = cast<GlobalAddressSDNode>(Op);
  assert(N->getOffset() == 0 && "unexpected offset in global node");
  return getAddr(N, DAG, N->getGlobal()->isDSOLocal());
}

SDValue SodiumTargetLowering::LowerBR_JT(SDValue Op, SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  SDValue Table = Op.getOperand(1);
  SDValue Index = Op.getOperand(2);
  SDLoc DL(Op);

  EVT PTy = getPointerTy(DAG.getDataLayout());

  JumpTableSDNode *JT = cast<JumpTableSDNode>(Table);
  SDValue JTI = DAG.getTargetJumpTable(JT->getIndex(), PTy);

  Table = DAG.getNode(SodiumISD::LA, DL, MVT::i16, JTI);
  Index = DAG.getNode(ISD::SHL, DL, PTy, Index, DAG.getConstant(2, DL, PTy));

  SDValue Addr = DAG.getNode(ISD::ADD, DL, PTy, Table, Index);
  if (isPositionIndependent()) {
    Addr = DAG.getLoad((EVT)MVT::i16, DL, Chain, Addr,
                    MachinePointerInfo::getJumpTable(DAG.getMachineFunction()));
    Chain = Addr.getValue(1);
    Addr = DAG.getNode(ISD::ADD, DL, PTy, Addr, Table);
  } else {
    Addr = DAG.getLoad(PTy, DL, Chain, Addr,
                    MachinePointerInfo::getJumpTable(DAG.getMachineFunction()));
    Chain = Addr.getValue(1);
  }
  return DAG.getNode(ISD::BRIND, DL, MVT::Other, Chain, Addr);
}

SDValue SodiumTargetLowering::LowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  SodiumMachineFunctionInfo *SodiumFI = MF.getInfo<SodiumMachineFunctionInfo>();

  SDLoc DL(Op);
  SDValue FI = DAG.getFrameIndex(SodiumFI->getVarArgsFrameIndex(),
                                 getPointerTy(MF.getDataLayout()));

  // vastart just stores the address of the VarArgsFrameIndex slot into the
  // memory location argument.
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), DL, FI, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

static std::optional<bool> matchSetCC(SDValue LHS, SDValue RHS,
                                      ISD::CondCode CC, SDValue Val) {
  assert(Val->getOpcode() == ISD::SETCC);
  SDValue LHS2 = Val.getOperand(0);
  SDValue RHS2 = Val.getOperand(1);
  ISD::CondCode CC2 = cast<CondCodeSDNode>(Val.getOperand(2))->get();

  if (LHS == LHS2 && RHS == RHS2) {
    if (CC == CC2)
      return true;
    if (CC == ISD::getSetCCInverse(CC2, LHS2.getValueType()))
      return false;
  } else if (LHS == RHS2 && RHS == LHS2) {
    CC2 = ISD::getSetCCSwappedOperands(CC2);
    if (CC == CC2)
      return true;
    if (CC == ISD::getSetCCInverse(CC2, LHS2.getValueType()))
      return false;
  }
  return std::nullopt;
}

SDValue SodiumTargetLowering::LowerSelect(SDValue Op, SelectionDAG &DAG) const {
  SDNode *N = Op.getNode();
  SDValue CondV = N->getOperand(0);
  SDValue TrueV = N->getOperand(1);
  SDValue FalseV = N->getOperand(2);
  MVT VT = N->getSimpleValueType(0);

  // Try to fold (select (setcc lhs, rhs, cc), truev, falsev) into bitwise ops
  // when both truev and falsev are also setcc.
  SDLoc DL(N);
  if (CondV.getOpcode() == ISD::SETCC && TrueV.getOpcode() == ISD::SETCC &&
      FalseV.getOpcode() == ISD::SETCC) {
    SDValue LHS = CondV.getOperand(0);
    SDValue RHS = CondV.getOperand(1);
    ISD::CondCode CC = cast<CondCodeSDNode>(CondV.getOperand(2))->get();

    // (select x, x, y) -> x | y
    // (select !x, x, y) -> x & y
    if (std::optional<bool> MatchResult = matchSetCC(LHS, RHS, CC, TrueV)) {
      return DAG.getNode(*MatchResult ? ISD::OR : ISD::AND, DL, VT, TrueV,
                         DAG.getFreeze(FalseV));
    }
    // (select x, y, x) -> x & y
    // (select !x, y, x) -> x | y
    if (std::optional<bool> MatchResult = matchSetCC(LHS, RHS, CC, FalseV)) {
      return DAG.getNode(*MatchResult ? ISD::AND : ISD::OR, DL, VT,
                         DAG.getFreeze(TrueV), FalseV);
    }
  }
  return SDValue();
}

SDValue SodiumTargetLowering::LowerMUL_LOHI(SDValue Op, SelectionDAG &DAG, bool isSigned) const {
  if(Op.getValueType() != MVT::i16) return SDValue();

  SDLoc DL(Op);

  // Use Intrinsic which would auto parsed via tablegen.
  unsigned Opcode = isSigned ? Intrinsic::sodium_mul32 : Intrinsic::sodium_mulu32;
  SDValue Op0 = DAG.getNode(ISD::INTRINSIC_WO_CHAIN, DL, MVT::v2i16,
                            DAG.getConstant(Opcode, DL, MVT::i16),
                            Op->getOperand(0), Op.getOperand(1));
  // Extract Lo and Hi from IntPair Results.
  SDValue Lows  = DAG.getNode(ISD::EXTRACT_VECTOR_ELT, DL, MVT::i16,
                              Op0, DAG.getConstant(0, DL, MVT::i16));
  SDValue Highs = DAG.getNode(ISD::EXTRACT_VECTOR_ELT, DL, MVT::i16,
                              Op0, DAG.getConstant(1, DL, MVT::i16));

  SDValue Ops[] = {Lows, Highs};
  return DAG.getMergeValues(Ops, DL);
}
