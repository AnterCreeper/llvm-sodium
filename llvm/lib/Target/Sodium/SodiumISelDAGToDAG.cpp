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

  // Expand to a constant pool using the default expansion code.
  return SDValue();
}

#include "SodiumISelDAGToDAGOpt.h"

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
  }
  // Select the default instruction.
  SelectCode(Node);
}

// This pass converts a legalized DAG into a RISCV-specific DAG, ready
// for instruction scheduling.
FunctionPass *llvm::createSodiumISelDag(SodiumTargetMachine &TM,
                                        CodeGenOpt::Level OptLevel) {
  return new SodiumDAGToDAGISel(TM, OptLevel);
}

char SodiumDAGToDAGISel::ID = 0;

INITIALIZE_PASS(SodiumDAGToDAGISel, DEBUG_TYPE, PASS_NAME, false, false)
