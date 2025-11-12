//===-- SodiumISelLowering.h - Sodium DAG Lowering Interface ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that Sodium uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SODIUM_SODIUMISELLOWERING_H
#define LLVM_SODIUM_SODIUMISELLOWERING_H

#include "Sodium.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"

namespace llvm {
namespace SodiumISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
  LI,
  LA,
  Call,
  Tail,
  Ret,
  ERet,
  TBE,
};
} // end namespace SodiumISD

class SodiumSubtarget;
class SodiumTargetLowering : public TargetLowering {
  const SodiumSubtarget &Subtarget;

public:
  explicit SodiumTargetLowering(const TargetMachine &TM, const SodiumSubtarget &STI);

  const char *getTargetNodeName(unsigned Opcode) const override;

  void ReplaceNodeResults(SDNode *N, SmallVectorImpl<SDValue>& Results,
                          SelectionDAG &DAG) const override;
  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;
  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

  SDValue LowerCall(TargetLowering::CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;
  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals,
                      const SDLoc &dl, SelectionDAG &DAG) const override;
  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &dl, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  bool useSoftFloat() const override { return true; }
  bool shouldNormalizeToSelectSequence(LLVMContext &, EVT) const override {
    return false;
  };
  virtual bool requiresDiffExpressionRelocations() const { return false; }

  Register getExceptionPointerRegister(const Constant *PersonalityFn) const override {
    return Sodium::X10;
  }
  Register getExceptionSelectorRegister(const Constant *PersonalityFn) const override {
    return Sodium::X11;
  }

  ConstraintType getConstraintType(StringRef Constraint) const override;
  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;

  void LowerAsmOperandForConstraint(SDValue Op, std::string &Constraint,
                                    std::vector<SDValue> &Ops,
                                    SelectionDAG &DAG) const override;
  bool isLegalAddressingMode(const DataLayout &DL, const AddrMode &AM, Type *Ty,
                             unsigned AS,
                             Instruction *I = nullptr) const override;

private:
  template<typename T>
  void analyzeArgs(const SmallVectorImpl<T> &Args, CCState &CCInfo) const;
  bool isEligibleForTailCallOptimization(CCState &CCInfo,
                                         CallLoweringInfo &CLI, MachineFunction &MF,
                                         const SmallVector<CCValAssign, 16> &ArgLocs) const;
  SDValue LowerCallResult(TargetLowering::CallLoweringInfo &CLI, SDValue Chain,
                          const SmallVectorImpl<ISD::InputArg> &Ins,
                          const SDLoc &DL, SelectionDAG &DAG,
                          SmallVectorImpl<SDValue> &InVals) const;

  template <class NodeTy>
  SDValue getAddr(NodeTy *N, SelectionDAG &DAG, bool IsLocal = true) const;
  template<typename T>
  SDValue LowerSimpleAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;

  SDValue LowerSelect(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBR_JT(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerMUL_LOHI(SDValue Op, SelectionDAG &DAG, bool isSigned) const;
};

//Configuration of DAGCombine
static const ISD::NodeType ISD_COMBINE[] = {
  ISD::ADD, ISD::SUB,
  ISD::AND, ISD::OR, ISD::XOR,
  ISD::MUL,
  ISD::LOAD, ISD::STORE,
  ISD::BR_CC, ISD::SELECT_CC
};

//Configuration of integer LowerOperation
static const unsigned ISD_LEGAL[] = {
  ISD::CTLZ, ISD::CTTZ,
  ISD::SMIN, ISD::SMAX, ISD::UMIN, ISD::UMAX
};
static const unsigned ISD_EXPAND[] = {
  ISD::BR_CC, ISD::SELECT_CC,
  ISD::CTPOP, ISD::PARITY,
  ISD::SDIV, ISD::UDIV, ISD::SREM, ISD::UREM,
  ISD::SDIVREM, ISD::UDIVREM,
  ISD::SHL_PARTS, ISD::SRA_PARTS, ISD::SRL_PARTS
};
static const unsigned ISD_CUSTOM[] = {
  ISD::SMUL_LOHI, ISD::UMUL_LOHI,
  ISD::GlobalAddress, ISD::BlockAddress, ISD::ConstantPool, ISD::JumpTable
};

} // end namespace llvm

#endif // LLVM_SODIUM_SODIUMISELLOWERING_H
