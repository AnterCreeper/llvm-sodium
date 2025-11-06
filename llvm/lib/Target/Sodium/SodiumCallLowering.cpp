//===---- SodiumCallLowering.cpp - Sodium DAG Lowering Implementation -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is part of SodiumISelLowering.cpp, contains the lowerCall
//  series function.
//
//===----------------------------------------------------------------------===//

#include "SodiumISelLowering.h"
#include "SodiumRegisterInfo.h"
#include "SodiumSubtarget.h"
#include "SodiumTargetMachine.h"
#include "MCTargetDesc/SodiumBaseInfo.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Analysis/VectorUtils.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>
#include <deque>

using namespace llvm;

#define DEBUG_TYPE "sodium-lowercall"

#include "SodiumGenCallingConv.inc"

bool SodiumTargetLowering::
isEligibleForTailCallOptimization(CCState &CCInfo,
                                  CallLoweringInfo &CLI, MachineFunction &MF,
                                  const SmallVector<CCValAssign, 16> &ArgLocs) const {
  auto CalleeCC = CLI.CallConv;
  auto &Outs = CLI.Outs;
  auto &Caller = MF.getFunction();
  auto CallerCC = Caller.getCallingConv();

  // Exception-handling functions need a special set of instructions to
  // indicate a return to the hardware. Tail-calling another function would
  // probably break this.
  if (Caller.hasFnAttribute("interrupt"))
    return false;

  // Do not tail call opt if the stack is used to pass parameters.
  if (CCInfo.getStackSize() != 0)
    return false;

  // Do not tail call opt if any parameters need to be passed indirectly.
  for (auto &VA : ArgLocs)
    if (VA.getLocInfo() == CCValAssign::Indirect)
      return false;

  // Do not tail call opt if either caller or callee uses struct return
  // semantics.
  auto IsCallerStructRet = Caller.hasStructRetAttr();
  auto IsCalleeStructRet = Outs.empty() ? false : Outs[0].Flags.isSRet();
  if (IsCallerStructRet || IsCalleeStructRet)
    return false;

  // The callee has to preserve all registers the caller needs to preserve.
  const SodiumRegisterInfo *TRI = Subtarget.getRegisterInfo();
  const uint32_t *CallerPreserved = TRI->getCallPreservedMask(MF, CallerCC);
  if (CalleeCC != CallerCC) {
    const uint32_t *CalleePreserved = TRI->getCallPreservedMask(MF, CalleeCC);
    if (!TRI->regmaskSubsetEqual(CallerPreserved, CalleePreserved))
      return false;
  }

  // Byval parameters hand the function a pointer directly into the stack area
  // we want to reuse during a tail call. Working around this *is* possible
  // but less efficient and uglier in LowerCall.
  for (auto &Arg : Outs)
    if (Arg.Flags.isByVal())
      return false;

  return true;
}

SDValue
SodiumTargetLowering::LowerCallResult(TargetLowering::CallLoweringInfo &CLI, SDValue Chain,
                                      const SmallVectorImpl<ISD::InputArg> &Ins,
                                      const SDLoc &DL, SelectionDAG &DAG,
                                      SmallVectorImpl<SDValue> &InVals) const {
  // Assign locations to each value returned by this call.
  SDValue Glue = Chain.getValue(1);
  SmallVector<CCValAssign, 16> RetLocs;
  CCState RetCCInfo(CLI.CallConv, CLI.IsVarArg, DAG.getMachineFunction(), RetLocs,
                    *DAG.getContext());
  analyzeArgs<ISD::InputArg>(Ins, RetCCInfo);
  // Copy all of the result registers out of their specified physreg.
  for (unsigned I = 0, E = RetLocs.size(); I != E; ++I) {
    assert(RetLocs[I].isRegLoc());
    SDValue Val = DAG.getCopyFromReg(Chain, DL, RetLocs[I].getLocReg(),
                                     RetLocs[I].getLocVT(), Glue);
    Chain = Val.getValue(1);
    Glue = Val.getValue(2);
    if (RetLocs[I].getValVT() != RetLocs[I].getLocVT())
      Val = DAG.getNode(ISD::BITCAST, DL, RetLocs[I].getValVT(), Val);
    InVals.push_back(Val);
  }
  return Chain;
}

// Lower a call to a callseq_start + CALL + callseq_end chain, and add input
// and output parameter nodes.
SDValue
SodiumTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                SmallVectorImpl<SDValue> &InVals) const {

  SDLoc DL = CLI.DL;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  bool &IsTailCall = CLI.IsTailCall;

  SelectionDAG &DAG = CLI.DAG;
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  // NOTE: Outs is args of calling
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState ArgCCInfo(CLI.CallConv, CLI.IsVarArg, DAG.getMachineFunction(), ArgLocs,
                    *DAG.getContext());
  analyzeArgs<ISD::OutputArg>(Outs, ArgCCInfo);

  // Check if it's really possible to do a tail call.
  if (IsTailCall)
    IsTailCall = isEligibleForTailCallOptimization(ArgCCInfo, CLI, MF, ArgLocs);
  //TODO

  std::deque<std::pair<unsigned, SDValue>> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;

  for (unsigned I = 0, E = ArgLocs.size(); I != E; ++I) {
    SDValue Arg = OutVals[I];
    CCValAssign &VA = ArgLocs[I];
    // Promote the value if needed.
    switch (VA.getLocInfo()) {
      default: llvm_unreachable("Unknown loc info!");
      case CCValAssign::Full:
        break;
      case CCValAssign::SExt:
        Arg = DAG.getNode(ISD::SIGN_EXTEND, DL, VA.getLocVT(), Arg);
        break;
      case CCValAssign::ZExt:
        Arg = DAG.getNode(ISD::ZERO_EXTEND, DL, VA.getLocVT(), Arg);
        break;
      case CCValAssign::AExt:
        Arg = DAG.getNode(ISD::ANY_EXTEND, DL, VA.getLocVT(), Arg);
        break;
    }
    // Arguments that can be passed on register must be kept at
    // RegsToPass vector
    if (VA.isRegLoc()) {
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
      continue;
    } else if (VA.isMemLoc()) {
      SDValue StackBase = DAG.getCopyFromReg(Chain, DL, Sodium::X4,
                                             getPointerTy(DAG.getDataLayout()));
      SDValue StackPtr = DAG.getNode(ISD::ADD, DL, getPointerTy(DAG.getDataLayout()),
                                     StackBase, DAG.getIntPtrConstant(VA.getLocMemOffset(), DL));

      // emit ISD::STORE whichs stores the parameter value to a stack location
      MemOpChains.push_back(DAG.getStore(Chain, DL, Arg, StackPtr, MachinePointerInfo()));
      MFI.setOffsetAdjustment(VA.getLocMemOffset() + 4);
    }
  }

  // Transform all store nodes into one single node because all store
  // nodes are independent of each other.
  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  // If the callee is a GlobalAddress/ExternalSymbol node (quite common, every
  // direct call is) turn it into a TargetGlobalAddress/TargetExternalSymbol
  // node so that legalize doesn't hack it.
  EVT PtrVT = getPointerTy(DAG.getDataLayout());

  // If the callee is a GlobalAddress node (quite common, every direct call is)
  // turn it into a TargetGlobalAddress node so that legalize doesn't hack it.
  // Likewise ExternalSymbol -> TargetExternalSymbol.
  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), DL, PtrVT, SODIUMII::MO_CALL);
  else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), PtrVT, SODIUMII::MO_CALL);

  // The first call operand is the chain and the second is the target address.
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  SDValue Glue;
  for (unsigned I = 0, E = RegsToPass.size(); I != E; ++I) {
    Chain = CLI.DAG.getCopyToReg(Chain, CLI.DL, RegsToPass[I].first,
                                 RegsToPass[I].second, Glue);
    Glue = Chain.getValue(1);
    Ops.push_back(CLI.DAG.getRegister(RegsToPass[I].first,
                                      RegsToPass[I].second.getValueType()));
  }

  // Add a register mask operand representing the call-preserved registers.
  const TargetRegisterInfo *TRI = Subtarget.getRegisterInfo();
  const uint32_t *Mask = TRI->getCallPreservedMask(MF, CLI.CallConv);
  assert(Mask && "Missing call preserved mask for calling convention");
  Ops.push_back(DAG.getRegisterMask(Mask));

  if (Glue.getNode()) {
    Ops.push_back(Glue);
  }

  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  Chain = DAG.getNode(SodiumISD::Call, DL, NodeTys, Ops);

  return LowerCallResult(CLI, Chain, Ins, DL, DAG, InVals);
}
