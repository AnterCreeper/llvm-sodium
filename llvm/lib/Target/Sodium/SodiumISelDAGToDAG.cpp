//=- SodiumISelDAGToDAG.cpp - A dag to dag inst selector for Sodium -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the Sodium target.
//
//===----------------------------------------------------------------------===//

#include "SodiumISelDAGToDAG.h"
#include "SodiumISelLowering.h"
#include "llvm/Support/KnownBits.h"
#include "MCTargetDesc/SodiumMCTargetDesc.h"

using namespace llvm;

#define DEBUG_TYPE "sodium-isel"
#define PASS_NAME "Sodium DAG->DAG Pattern Instruction Selection"

static SDValue SelectImm(SelectionDAG *CurDAG, const SDLoc &DL, const MVT VT,
                         int64_t Imm, const SodiumSubtarget &Subtarget) {
  if (isInt<13>(Imm)) {
    SDValue SDImm = CurDAG->getTargetConstant(Imm, DL, VT);
    return SDValue(CurDAG->getMachineNode(Sodium::ADDI, DL, VT,
                                         CurDAG->getRegister(Sodium::X0, VT), SDImm), 0);
  } else {
    int64_t Hi19 = ((Imm + 0x1000) >> 13) & (Subtarget.is32Bit ? 0x7FFFF : 0x7);
    int64_t Lo13 = SignExtend32<13>(Imm);
    SDValue LuiOp = SDValue(CurDAG->getMachineNode(Sodium::LUI, DL, VT,
                                                  CurDAG->getTargetConstant(Hi19, DL, VT)), 0);
    if (Lo13 == 0) return LuiOp;
    SDValue AddiOp = SDValue(CurDAG->getMachineNode(Sodium::ADDI, DL, VT, LuiOp,
                                                   CurDAG->getTargetConstant(Lo13, DL, VT)), 0);
    return AddiOp;
  }
  return SDValue();
}

bool SodiumDAGToDAGISel::SelectInlineAsmMemoryOperand(const SDValue &Op, unsigned ConstraintID,
                                                      std::vector<SDValue> &OutOps) {
  // Always produce a register and immediate operand, as expected by
  // SodiumAsmPrinter::PrintAsmMemoryOperand.
  SDValue Base = Op;
  SDValue Offset =
    CurDAG->getTargetConstant(0, SDLoc(Op), Op.getValueType());
  switch (ConstraintID) {
  default:
    report_fatal_error("Unexpected asm memory constraint " +
                       InlineAsm::getMemConstraintName(ConstraintID));
  // Reg+simm13 addressing.
  case InlineAsm::Constraint_o:
  case InlineAsm::Constraint_m:
    if (CurDAG->isBaseWithConstantOffset(Op)) {
      ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Op.getOperand(1));
      if (isIntN(13, CN->getSExtValue())) {
        Base = Op.getOperand(0);
        Offset = CurDAG->getTargetConstant(CN->getZExtValue(), SDLoc(Op),
                                           Op.getValueType());
      }
    }
    break;
  case InlineAsm::Constraint_A:
    break;
  }
  OutOps.push_back(Base);
  OutOps.push_back(Offset);
  return false;
}

#include "SodiumISelDAGToDAGOpt.h"

#define SelectSodium(Opc) \
  ReplaceNode(Node, CurDAG->getMachineNode((Opc), DL, VT, Node->getOperand(0), Node->getOperand(1)));

void SodiumDAGToDAGISel::Select(SDNode *Node) {
  if (Node->isMachineOpcode()) {
    Node->setNodeId(-1);
    return;
  }
  SDLoc DL(Node);
  MVT VT = Node->getSimpleValueType(0);
  switch (Node->getOpcode()) {
    default: break;
  case ISD::Constant: {
    auto *ConstNode = cast<ConstantSDNode>(Node);
    if (ConstNode->isZero()) {
      SDValue New =
          CurDAG->getCopyFromReg(CurDAG->getEntryNode(), DL, Sodium::X0, VT);
      ReplaceNode(Node, New.getNode());
      return;
    }
    int64_t Imm = ConstNode->getSExtValue();
    ReplaceNode(Node, SelectImm(CurDAG, DL, VT, Imm, *Subtarget).getNode());
    return;
  }
  case ISD::FrameIndex: {
    SDValue Imm = CurDAG->getTargetConstant(0, DL, MVT::i32);
    int FI = cast<FrameIndexSDNode>(Node)->getIndex();
    SDValue TFI = CurDAG->getTargetFrameIndex(FI, VT);
    ReplaceNode(Node, CurDAG->getMachineNode(Sodium::ADDI, DL, VT, TFI, Imm));
    return;
  }
  case ISD::OR:
    if (tryBitfieldPackfromOrSHL(CurDAG, Node)) return;
    if (tryShrinkShlLogicImm(CurDAG, Node, Sodium::ORI)) return;
    break;
  case ISD::XOR:
    if (tryShrinkShlLogicImm(CurDAG, Node, Sodium::XORI)) return;
    break;
  case ISD::AND:
    if (tryBitfieldOpfromAND(CurDAG, Node)) return;
    if (tryShrinkShlLogicImm(CurDAG, Node, Sodium::ANDI)) return;
    break;
  case ISD::SRA:
    if (tryBitfieldOpfromSHR(CurDAG, Node, true)) return;
    break;
  case ISD::SRL:
    if (tryBitfieldOpfromSHR(CurDAG, Node, false)) return;
    break;
  case ISD::SHL:
    if (tryBitfieldInsertOpfromSHL(CurDAG, Node)) return;
    break;
  case ISD::SIGN_EXTEND_INREG:
    if (tryBitfieldOpfromSExtInReg(CurDAG, Node)) return;
    break;
  case SodiumISD::Mul32: {
    SelectSodium(Sodium::MULD);
    return;
  }
  case SodiumISD::Mulu32: {
    SelectSodium(Sodium::MULDU);
    return;
  }
  }
  // Select the default instruction.
  SelectCode(Node);
}

// This pass converts a legalized DAG into a Sodium-specific DAG, ready
// for instruction scheduling.
FunctionPass *llvm::createSodiumISelDag(SodiumTargetMachine &TM,
                                        CodeGenOpt::Level OptLevel) {
  return new SodiumDAGToDAGISel(TM, OptLevel);
}

char SodiumDAGToDAGISel::ID = 0;

INITIALIZE_PASS(SodiumDAGToDAGISel, DEBUG_TYPE, PASS_NAME, false, false)
