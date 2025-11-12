static SDValue combineAddOfBooleanXor(SDNode *N, SelectionDAG &DAG) {
  SDValue N0 = N->getOperand(0);
  SDValue N1 = N->getOperand(1);
  EVT VT = N->getValueType(0);

  // RHS should be -1.
  if (!isAllOnesConstant(N1))
    return SDValue();

  // Look for an (xor (setcc X, Y), 1).
  if (N0.getOpcode() != ISD::XOR || !isOneConstant(N0.getOperand(1)) ||
      N0.getOperand(0).getOpcode() != ISD::SETCC)
    return SDValue();

  // Emit a negate of the setcc.
  SDLoc DL(N);
  return DAG.getNode(ISD::SUB, DL, VT, DAG.getConstant(0, DL, VT),
                     N0.getOperand(0));
}

//combine and+sll into shadd
static SDValue combineAddShlImm(SDNode *N, SelectionDAG &DAG) {
  // Skip for larger types.
  EVT VT = N->getValueType(0);
  if (VT.getSizeInBits() > 16) return SDValue();

  // The two operand nodes must be SHL and have no other use.
  SDValue N0 = N->getOperand(0);
  SDValue N1 = N->getOperand(1);
  if (N0->getOpcode() != ISD::SHL || N1->getOpcode() != ISD::SHL ||
      !N0->hasOneUse() || !N1->hasOneUse())
    return SDValue();

  // Check c0 and c1.
  auto *N0C = dyn_cast<ConstantSDNode>(N0->getOperand(1));
  auto *N1C = dyn_cast<ConstantSDNode>(N1->getOperand(1));
  if (!N0C || !N1C)
    return SDValue();
  int64_t C0 = N0C->getSExtValue();
  int64_t C1 = N1C->getSExtValue();
  if (C0 <= 0 || C1 <= 0)
    return SDValue();

  // Skip if shamt are out of range.
  int64_t Bits = std::min(C0, C1);
  int64_t Diff = std::abs(C0 - C1);
  if (Diff > 8) return SDValue();

  // Build nodes.
  SDLoc DL(N);
  SDValue NS = (C0 < C1) ? N0->getOperand(0) : N1->getOperand(0);
  SDValue NL = (C0 > C1) ? N0->getOperand(0) : N1->getOperand(0);
  SDValue NA0 =
      DAG.getNode(ISD::SHL, DL, VT, NL, DAG.getConstant(Diff, DL, VT));
  SDValue NA1 = DAG.getNode(ISD::ADD, DL, VT, NA0, NS);
  return DAG.getNode(ISD::SHL, DL, VT, NA1, DAG.getConstant(Bits, DL, VT));
}

// Apply DeMorgan's law to (and/or (xor X, 1), (xor Y, 1)) if X and Y are 0/1.
// Legalizing setcc can introduce xors like this. Doing this transform reduces
// the number of xors and may allow the xor to fold into a branch condition.
static SDValue combineDeMorganOfBoolean(SDNode *N, SelectionDAG &DAG) {
  SDValue N0 = N->getOperand(0);
  SDValue N1 = N->getOperand(1);
  EVT VT = N->getValueType(0);
  bool IsAnd = N->getOpcode() == ISD::AND;

  if (N0.getOpcode() != ISD::XOR || N1.getOpcode() != ISD::XOR)
    return SDValue();

  if (!N0.hasOneUse() || !N1.hasOneUse())
    return SDValue();

  SDValue N01 = N0.getOperand(1);
  SDValue N11 = N1.getOperand(1);

  // For AND, SimplifyDemandedBits may have turned one of the (xor X, 1) into
  // (xor X, -1) based on the upper bits of the other operand being 0. If the
  // operation is And, allow one of the Xors to use -1.
  if (isOneConstant(N01)) {
    if (!isOneConstant(N11) && !(IsAnd && isAllOnesConstant(N11)))
      return SDValue();
  } else if (isOneConstant(N11)) {
    // N01 and N11 being 1 was already handled. Handle N11==1 and N01==-1.
    if (!(IsAnd && isAllOnesConstant(N01)))
      return SDValue();
  } else
    return SDValue();


  SDValue N00 = N0.getOperand(0);
  SDValue N10 = N1.getOperand(0);

  // The LHS of the xors needs to be 0/1.
  APInt Mask = APInt::getBitsSetFrom(VT.getSizeInBits(), 1);
  if (!DAG.MaskedValueIsZero(N00, Mask) || !DAG.MaskedValueIsZero(N10, Mask))
    return SDValue();

  // Invert the opcode and insert a new xor.
  SDLoc DL(N);
  unsigned Opc = IsAnd ? ISD::OR : ISD::AND;
  SDValue Logic = DAG.getNode(Opc, DL, VT, N00, N10);
  return DAG.getNode(ISD::XOR, DL, VT, Logic, DAG.getConstant(1, DL, VT));
}

// optimize boolean logic to select which will be legalized into cond move.
static SDValue combineAndSetCCToCMOV(SDNode *N, SelectionDAG &DAG) {
  if (N->getOpcode() != ISD::AND)
    return SDValue();

  SDValue N0 = N->getOperand(0);
  SDValue N1 = N->getOperand(1);
  EVT VT = N->getValueType(0);

  auto IsEqualCompZero = [](SDValue &V) -> bool {
    if (V.getOpcode() == ISD::SETCC && isNullConstant(V.getOperand(1))) {
      ISD::CondCode CC = cast<CondCodeSDNode>(V.getOperand(2))->get();
      if (ISD::isIntEqualitySetCC(CC))
        return true;
    }
    return false;
  };

  if (!IsEqualCompZero(N0) || !N0.hasOneUse())
    std::swap(N0, N1);
  if (!IsEqualCompZero(N0) || !N0.hasOneUse())
    return SDValue();

  KnownBits Known = DAG.computeKnownBits(N1);
  if (Known.getMaxValue().ugt(1))
    return SDValue();

  SDLoc DL(N);
  return DAG.getNode(ISD::SELECT, DL, VT, N0, N1, DAG.getConstant(0, DL, VT));
}

static SDValue expandMULtoSHLADD(SDNode *N, TargetLowering::DAGCombinerInfo &DCI,
                                 SDValue X, ConstantSDNode* C) {
  SelectionDAG &DAG = DCI.DAG;

  SDLoc DL(N);
  EVT VT = N->getValueType(0);

  int64_t MulAmt = C->getSExtValue();
  unsigned ShiftAmt = llvm::countr_zero<uint64_t>(MulAmt) & (16 - 1);
  MulAmt >>= ShiftAmt;

  SDValue Res;
  if (MulAmt >= 0) {
    if (llvm::has_single_bit<uint32_t>(MulAmt - 1)) {
      // fold mul x, 2^N + 1 => add (shl x, N), x
      Res = DAG.getNode(ISD::ADD, DL, VT,
                        DAG.getNode(ISD::SHL, DL, VT,
                                    X,
                                    DAG.getConstant(Log2_32(MulAmt - 1), DL, MVT::i16)),
                        X);
    } else if (llvm::has_single_bit<uint32_t>(MulAmt + 1)) {
      // fold mul x, 2^N - 1 => sub (shl x, N), x
      Res = DAG.getNode(ISD::SUB, DL, VT,
                        DAG.getNode(ISD::SHL, DL, VT,
                                    X,
                                    DAG.getConstant(Log2_32(MulAmt + 1), DL, MVT::i16)),
                        X);
    } else
      return SDValue();
  } else {
    uint64_t MulAmtAbs = -MulAmt;
    if (llvm::has_single_bit<uint32_t>(MulAmtAbs + 1)) {
      // fold mul x, -(2^N - 1) => sub x, (shl x, N)
      Res = DAG.getNode(ISD::SUB, DL, VT,
                        X,
                        DAG.getNode(ISD::SHL, DL, VT,
                                    X,
                                    DAG.getConstant(Log2_32(MulAmtAbs + 1), DL, MVT::i16)));
    } else
      return SDValue();
  }

  if (ShiftAmt != 0)
    Res = DAG.getNode(ISD::SHL, DL, VT,
                      Res,
                      DAG.getConstant(ShiftAmt, DL, MVT::i16));

  // Do not add new nodes to DAG combiner worklist.
  DCI.CombineTo(N, Res, false);
  return SDValue();
}

//===----------------------------------------------------------------------===//
// DAGCombine
//===----------------------------------------------------------------------===//
static SDValue performADDCombine(SDNode *N, SelectionDAG &DAG) {
  // fold add (shl x, c0), (shl y, c1) =>
  //      SLLI (SHADD x, y, diff), c0, if c1-c0 within 1 to 8.
  if (SDValue V = combineAddShlImm(N, DAG))
    return V;
  // fold add (xor (setcc X, Y), 1), -1 => neg (setcc X, Y).
  if (SDValue V = combineAddOfBooleanXor(N, DAG))
    return V;
  return SDValue();
}

static SDValue performSUBCombine(SDNode *N, SelectionDAG &DAG) {
  SDValue N0 = N->getOperand(0);
  SDValue N1 = N->getOperand(1);
  // fold sub 0, (setcc x, 0, setlt) => sra x, xlen - 1
  if (isNullConstant(N0) && N1.getOpcode() == ISD::SETCC && N1.hasOneUse() &&
      isNullConstant(N1.getOperand(1))) {
    ISD::CondCode CCVal = cast<CondCodeSDNode>(N1.getOperand(2))->get();
    if (CCVal == ISD::SETLT) {
      EVT VT = N->getValueType(0);
      SDLoc DL(N);
      unsigned ShAmt = N0.getValueSizeInBits() - 1;
      return DAG.getNode(ISD::SRA, DL, VT, N1.getOperand(0),
                         DAG.getConstant(ShAmt, DL, VT));
    }
  }
  return SDValue();
}

static SDValue performMULCombine(SDNode *N,
                                 TargetLowering::DAGCombinerInfo &DCI) {
  SDLoc DL(N);
  ConstantSDNode *C = dyn_cast<ConstantSDNode>(N->getOperand(1));
  if (!C) return SDValue();

  SDValue X = N->getOperand(0);
  //fold mul x, C => and/sub (shl x, ShiftAmt), (-)x
  if (SDValue V = expandMULtoSHLADD(N, DCI, X, C))
    return V;

  return SDValue();
}

static SDValue performLogicCombine(SDNode *N,
                                   TargetLowering::DAGCombinerInfo &DCI) {
  SelectionDAG &DAG = DCI.DAG;
  if (DCI.isAfterLegalizeDAG()) {
    // fold and (setcc c, 0, ne), (i1)f => select (c, 0, ne), f, 0 => f = (movz c, zero)
    //      and (setcc c, 0, eq), (i1)f => select (c, 0, eq), f, 0 => f = (movn c, zero)
    if (SDValue V = combineAndSetCCToCMOV(N, DAG))
      return V;
    if (SDValue V = combineDeMorganOfBoolean(N, DAG))
      return V;
  }
  return SDValue();
}

static SDValue performXORCombine(SDNode *N, SelectionDAG &DAG) {
  SDValue N0 = N->getOperand(0);
  SDValue N1 = N->getOperand(1);
  // fold xor (sll 1, x), -1 => rol ~1, x
  if (N0.getOpcode() == ISD::SHL &&
      isAllOnesConstant(N1) && isOneConstant(N0.getOperand(0))) {
    SDLoc DL(N);
    EVT VT = N->getValueType(0);
    return DAG.getNode(ISD::ROTL, DL, VT,
                       DAG.getConstant(~1, DL, VT), N0.getOperand(1));
  }
  // fold xor (setcc constant, y, setlt), 1 => setcc y, constant + 1, setlt
  if (N0.hasOneUse() && N0.getOpcode() == ISD::SETCC && isOneConstant(N1)) {
    auto *ConstN00 = dyn_cast<ConstantSDNode>(N0.getOperand(0));
    ISD::CondCode CC = cast<CondCodeSDNode>(N0.getOperand(2))->get();
    if (ConstN00 && CC == ISD::SETLT) {
      EVT VT = N0.getValueType();
      SDLoc DL(N0);
      const APInt &Imm = ConstN00->getAPIntValue();
      if ((Imm + 1).isSignedIntN(13))
        return DAG.getSetCC(DL, VT, N0.getOperand(1),
                            DAG.getConstant(Imm + 1, DL, VT), CC);
    }
  }
  return SDValue();
}

// Try to combine two adjacent loads/stores to a single pair instruction.
static SDValue performMemPairCombine(SDNode *N,
                                     TargetLowering::DAGCombinerInfo &DCI) {
  //TODO
  return SDValue();
}

// Perform common combines for BR_CC and SELECT_CC condtions.
static bool performCCCombine(SDValue &LHS, SDValue &RHS, SDValue &CC,
                             const SDLoc &DL, SelectionDAG &DAG) {
  ISD::CondCode CCVal = cast<CondCodeSDNode>(CC)->get();
  // fold setlt (sra X, N), 0 => setlt X, 0 and
  //      setge (sra X, N), 0 => setge X, 0
  if (auto *RHSConst = dyn_cast<ConstantSDNode>(RHS.getNode())) {
    if ((CCVal == ISD::SETGE || CCVal == ISD::SETLT) &&
        LHS.getOpcode() == ISD::SRA && RHSConst->isZero()) {
      LHS = LHS.getOperand(0);
      return true;
    }
  }
  return false;
}
