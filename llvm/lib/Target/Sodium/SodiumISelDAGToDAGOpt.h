static SDValue BitfieldInsert(SelectionDAG *CurDAG, SDValue X, unsigned Lsb, unsigned Len,
                              SDLoc DL, MVT VT, bool isSigned) {
  unsigned Opc = isSigned ? Sodium::SBFI : Sodium::UBFI;
  return SDValue(CurDAG->getMachineNode(Opc, DL, VT,
                                        X,
                                        CurDAG->getTargetConstant(Lsb, DL, VT),
                                        CurDAG->getTargetConstant(Len, DL, VT)), 0);
}

static SDValue BitfieldExtract(SelectionDAG *CurDAG, SDValue X, unsigned Lsb, unsigned Len,
                               SDLoc DL, MVT VT, bool isSigned) {
  unsigned Opc = isSigned ? Sodium::SBFX : Sodium::UBFX;
  return SDValue(CurDAG->getMachineNode(Opc, DL, VT,
                                        X,
                                        CurDAG->getTargetConstant(Lsb, DL, VT),
                                        CurDAG->getTargetConstant(Len, DL, VT)), 0);
}

#define IS_SHIFTED_MASK(x, y) (((x) & ((x) + (1 << y))) == 0)
#define IS_MASK(x) IS_SHIFTED_MASK(x, 0)

bool SodiumDAGToDAGISel::tryBitfieldOpfromSHR(SelectionDAG *CurDAG, SDNode *Node, bool isSigned) {
  SDValue N0 = Node->getOperand(0);
  if (!N0.hasOneUse())
    return false;
  auto *C2 = dyn_cast<ConstantSDNode>(Node->getOperand(1));
  if (!C2)
    return false;

  SDLoc DL(Node);
  MVT VT = Node->getSimpleValueType(0);

  // fold sr? (sll x, C1), C2 =>
  //      ?bf? x, lsb, len
  if (N0.getOpcode() == ISD::SHL) {
    SDValue X = N0.getOperand(0);
    auto *C1 = dyn_cast<ConstantSDNode>(N0->getOperand(1));
    if (!C1)
      return false;

    const unsigned LeftShAmt = C1->getZExtValue();
    const unsigned RightShAmt = C2->getZExtValue();

    if (LeftShAmt > RightShAmt) {
      // Bit Field Insert,  $rd[len:0] = $rs[lsb+len:lsb]
      const unsigned Lsb = LeftShAmt - RightShAmt;
      const unsigned Len = VT.getSizeInBits() - LeftShAmt - 1;
      ReplaceNode(Node, BitfieldInsert(CurDAG, X, Lsb, Len, DL, VT, isSigned).getNode());
    } else {
      // Bit Field Extract, $rd[lsb+len:lsb] = $rs[len:0]
      const unsigned Lsb = RightShAmt - LeftShAmt;
      const unsigned Len = VT.getSizeInBits() - RightShAmt - 1;
      ReplaceNode(Node, BitfieldExtract(CurDAG, X, Lsb, Len, DL, VT, isSigned).getNode());
    }
    return true;
  }

  // fold sr? (and x, C1), C2 =>
  //      ?bfx x, lsb, len
  if (N0.getOpcode() == ISD::AND) {
    SDValue X = N0.getOperand(0);
    auto *C1 = dyn_cast<ConstantSDNode>(N0->getOperand(1));
    if (!C1)
      return false;

    const unsigned Mask = C1->getZExtValue();
    const unsigned RightShAmt = C2->getZExtValue();
    // Make sure that this is a bitfield extraction.
    const unsigned Lsb = llvm::countr_zero(Mask);
    if (!IS_SHIFTED_MASK(Mask, Lsb)) return false;
    if (Lsb != RightShAmt) return false;

    const unsigned Len = llvm::bit_width(Mask) - Lsb;
    bool needSigned = isSigned && ((unsigned)llvm::bit_width(Mask) == VT.getSizeInBits());
    ReplaceNode(Node, BitfieldExtract(CurDAG, X, Lsb, Len, DL, VT, needSigned).getNode());
    return true;
  }

  // fold sra (sext_inreg x, _), C2
  //   => sbfx x, lsb, len
  if (N0.getOpcode() == ISD::SIGN_EXTEND_INREG && isSigned) {
    SDValue X = N0.getOperand(0);
    unsigned ExtSize =
      cast<VTSDNode>(N0.getOperand(1))->getVT().getSizeInBits();

    const unsigned RightShAmt = C2->getZExtValue();
    // Make sure that this is a bitfield extraction.
    if (ExtSize == 16) return false;

    const unsigned Lsb = RightShAmt;
    const unsigned Len = ExtSize - RightShAmt - 1;
    ReplaceNode(Node, BitfieldExtract(CurDAG, X, Lsb, Len, DL, VT, true).getNode());
    return true;
  }

  return false;
}

bool SodiumDAGToDAGISel::tryBitfieldOpfromAND(SelectionDAG *CurDAG, SDNode *Node) {
  SDValue N0 = Node->getOperand(0);
  if (!N0.hasOneUse())
    return false;
  auto *C2 = dyn_cast<ConstantSDNode>(Node->getOperand(1));
  if (!C2)
    return false;

  SDLoc DL(Node);
  MVT VT = Node->getSimpleValueType(0);

  //fold and (sr? x, C1), C2
  //     and (sr? (sext_inreg x, _) C1), C2
  //  => ubfx x, lsb, len
  if (N0.getOpcode() == ISD::SRA || N0.getOpcode() == ISD::SRL) {
    SDValue X = N0.getOperand(0);
    auto *C1 = dyn_cast<ConstantSDNode>(N0->getOperand(1));
    if (!C1)
      return false;

    unsigned XLen = VT.getSizeInBits();
    if(X.getOpcode() == ISD::SIGN_EXTEND_INREG) {
      XLen = cast<VTSDNode>(X.getOperand(1))->getVT().getSizeInBits();
      X = X.getOperand(0);
    }

    const unsigned Mask = C2->getZExtValue();
    const unsigned RightShAmt = C1->getZExtValue();
    // Make sure that this is a bitfield extraction.
    if (!IS_MASK(Mask)) return false;
    if (RightShAmt + llvm::countr_one(Mask) > XLen) return false;

    const unsigned Lsb = RightShAmt;
    const unsigned Len = llvm::countr_one(Mask) - 1;
    ReplaceNode(Node, BitfieldExtract(CurDAG, X, Lsb, Len, DL, VT, false).getNode());
    return true;
  }

  //fold and (shl x, C1), C2
  //  => ubfi x, lsb, len
  if (N0.getOpcode() == ISD::SHL) {
    SDValue X = N0.getOperand(0);
    auto *C1 = dyn_cast<ConstantSDNode>(N0->getOperand(1));
    if (!C1)
      return false;

    unsigned Mask = C2->getZExtValue();
    const unsigned LeftShAmt = C1->getZExtValue();
    // Make sure that this is a bitfield insert.
    const unsigned Lsb = LeftShAmt;
    Mask >>= Lsb;
    if (Mask == 0) return false;
    if (!IS_MASK(Mask)) return false;

    const unsigned Len = llvm::countr_one(Mask) - 1;
    ReplaceNode(Node, BitfieldInsert(CurDAG, X, Lsb, Len, DL, VT, false).getNode());
    return true;
  }

  return false;
}

bool SodiumDAGToDAGISel::tryBitfieldOpfromSExtInReg(SelectionDAG *CurDAG, SDNode *Node) {
  SDValue N0 = Node->getOperand(0);
  if (!N0.hasOneUse())
    return false;
  unsigned ExtSize =
    cast<VTSDNode>(Node->getOperand(1))->getVT().getSizeInBits();

  SDLoc DL(Node);
  MVT VT = Node->getSimpleValueType(0);

  //fold sext_inreg (sr? x, C1) _
  //  => sbfx x, lsb, len
  if (N0.getOpcode() == ISD::SRA || N0.getOpcode() == ISD::SRL) {
    SDValue X = N0.getOperand(0);
    auto *C1 = dyn_cast<ConstantSDNode>(N0->getOperand(1));
    if (!C1)
      return false;

    const unsigned RightShAmt = C1->getZExtValue();
    // Make sure that this is a bitfield extraction.
    if (RightShAmt + ExtSize > VT.getSizeInBits()) return false;

    const unsigned Lsb = RightShAmt;
    const unsigned Len = ExtSize - 1;
    ReplaceNode(Node, BitfieldExtract(CurDAG, X, Lsb, Len, DL, VT, true).getNode());
    return true;
  }

  //fold sext_inreg (sll x, C1) _
  //  => sbfi x, lsb, len
  if (N0.getOpcode() == ISD::SHL) {
    SDValue X = N0.getOperand(0);
    auto *C1 = dyn_cast<ConstantSDNode>(N0->getOperand(1));
    if (!C1)
      return false;

    const unsigned LeftShAmt = C1->getZExtValue();

    const unsigned Lsb = LeftShAmt;
    const unsigned Len = ExtSize - LeftShAmt - 1;
    //if (Len < 0) llvm_unreachable("unreachable, as the result should be constant zero.");

    ReplaceNode(Node, BitfieldInsert(CurDAG, X, Lsb, Len, DL, VT, true).getNode());
    return true;
  }

  return false;
}

bool SodiumDAGToDAGISel::tryBitfieldInsertOpfromSHL(SelectionDAG *CurDAG, SDNode *Node) {
  SDValue N0 = Node->getOperand(0);
  if (!N0.hasOneUse())
    return false;
  auto *C2 = dyn_cast<ConstantSDNode>(Node->getOperand(1));
  if (!C2)
    return false;

  SDLoc DL(Node);
  MVT VT = Node->getSimpleValueType(0);

  // fold sll (and x, C1), C2 =>
  //      ubfi x, lsb, len
  if (N0.getOpcode() == ISD::AND) {
    SDValue X = N0.getOperand(0);
    auto *C1 = dyn_cast<ConstantSDNode>(N0->getOperand(1));
    if (!C1)
      return false;

    const unsigned Mask = C1->getZExtValue();
    const unsigned LeftShAmt = C2->getZExtValue();

    // Make sure that this is a bitfield insert.
    if (!IS_MASK(Mask)) return false;

    const unsigned Lsb = LeftShAmt;
    const unsigned Len = llvm::countr_one(Mask) - 1;
    ReplaceNode(Node, BitfieldInsert(CurDAG, X, Lsb, Len, DL, VT, false).getNode());
    return true;
  }

  return false;
}

// pack $rd, $rs1, $rs2, $shamt => $rd = {$rs2[15-shamt:0], $rs1[$shamt-1:0]};
bool SodiumDAGToDAGISel::tryBitfieldPackfromOrSHL(SelectionDAG *CurDAG, SDNode *Node) {
  SDValue N0 = Node->getOperand(0);
  SDValue N1 = Node->getOperand(1);

  //fold or (sll y, C1), x
  //  => pack x, y, C1
  SDValue X, Y;
  if (N0.getOpcode() == ISD::SHL && N0.hasOneUse()) { X = N1;  Y = N0; }
  else
  if (N1.getOpcode() == ISD::SHL && N1.hasOneUse()) { X = N0;  Y = N1; }
  else return false;

  SDLoc DL(Node);
  MVT VT = Node->getSimpleValueType(0);

  auto *ShAmt = dyn_cast<ConstantSDNode>(Y->getOperand(1));
  if (!ShAmt)
    return false;

  uint64_t NotKnownZero = (~CurDAG->computeKnownBits(X).Zero).getZExtValue();
  uint64_t Lsb = ShAmt->getZExtValue();
  if (NotKnownZero & maskTrailingZeros<uint64_t>(Lsb)) return false;

  ReplaceNode(Node, CurDAG->getMachineNode(Sodium::PACK, DL, VT,
                                           X,
                                           Y.getOperand(0),
                                           CurDAG->getTargetConstant(Lsb, DL, VT)));
  return true;
}

// For operations of the form ??? (sll x, C1), C2, check if we can use
// ANDI/ORI/XORI by transforming it into sll (??? x (C2>>C1)), C1.
bool SodiumDAGToDAGISel::tryShrinkShlLogicImm(SelectionDAG *CurDAG, SDNode *Node, unsigned BinOpc) {
  SDValue N0 = Node->getOperand(0);
  if (N0.getOpcode() != ISD::SHL || !N0.hasOneUse())
    return false;

  ConstantSDNode *C2 = dyn_cast<ConstantSDNode>(Node->getOperand(1));
  if (!C2)
    return false;

  SDLoc DL(Node);
  MVT VT = Node->getSimpleValueType(0);

  int64_t Val = C2->getSExtValue();
  // Check if immediate can already use ANDI/ORI/XORI.
  if (isInt<13>(Val)) return false;

  SDValue X = N0.getOperand(0);
  ConstantSDNode *C1 = dyn_cast<ConstantSDNode>(N0.getOperand(1));
  if (!C1) return false;

  uint64_t ShAmt = C1->getZExtValue();

  // Make sure that we don't change the operation by removing bits.
  // This only matters for OR and XOR, AND is unaffected.
  uint64_t RemovedBitsMask = maskTrailingOnes<uint64_t>(ShAmt);
  if (Node->getOpcode() != ISD::AND && (Val & RemovedBitsMask) != 0)
    return false;

  int64_t ShiftedVal = Val >> ShAmt;
  if (!isInt<13>(ShiftedVal))
    return false;

  SDNode *BinOp = CurDAG->getMachineNode(BinOpc, DL, VT,
                                         X,
                                         CurDAG->getTargetConstant(ShiftedVal, DL, VT));
  SDNode *SLLI = CurDAG->getMachineNode(Sodium::SLLI, DL, VT,
                                        SDValue(BinOp, 0),
                                        CurDAG->getTargetConstant(ShAmt, DL, VT));
  ReplaceNode(Node, SLLI);
  return true;
}
