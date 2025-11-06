//===-- SodiumISelLowering.cpp - Sodium DAG Lowering Implementation -------===//
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

#include "SodiumISelLowering.h"
#include "SodiumRegisterInfo.h"
#include "SodiumSubtarget.h"
#include "SodiumTargetMachine.h"
#include "SodiumMachineFunctionInfo.h"
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

#define DEBUG_TYPE "sodium-lower"

SodiumTargetLowering::SodiumTargetLowering(const TargetMachine &TM,
                                           const SodiumSubtarget &STI)
    : TargetLowering(TM), Subtarget(STI) {
  addRegisterClass(MVT::i16, &Sodium::IntRegsRegClass);
  computeRegisterProperties(Subtarget.getRegisterInfo());

  setMinFunctionAlignment(Align(2));
  setStackPointerRegisterToSaveRestore(Sodium::X4);

  setJumpIsExpensive();
  setMinimumJumpTableEntries(5);
  setSchedulingPreference(Sched::Hybrid);

  setBooleanContents(ZeroOrOneBooleanContent);
  setBooleanVectorContents(ZeroOrOneBooleanContent);

  // Promotes for i1
  AddPromotedToType(ISD::SETCC, MVT::i1, MVT::i16);
  setLoadExtAction({ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD}, MVT::i16,
                   MVT::i1, Promote);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8, Expand);

  // Plenty of Settings.
  setOperationAction(ISD_LEGAL,  MVT::i16, Legal);
  setOperationAction(ISD_EXPAND, MVT::i16, Expand);
  setOperationAction(ISD_CUSTOM, MVT::i16, Custom);
  setTargetDAGCombine(ISD_COMBINE);

  // Expand jump table branches as address arithmetic followed by an
  // indirect jump.
  setOperationAction(ISD::BR_JT, MVT::Other, Custom);

  // Variable arguments.
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction({ISD::VAARG, ISD::VACOPY, ISD::VAEND}, MVT::Other, Expand);

  // Variable-sized objects.
  setOperationAction({ISD::STACKSAVE, ISD::STACKRESTORE}, MVT::Other, Expand);

  // Stack alloca.
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i16, Expand);

  setOperationAction(ISD::PREFETCH, MVT::Other, Legal);
  setOperationAction({ISD::TRAP, ISD::DEBUGTRAP}, MVT::Other, Legal);

  // v2i16 stuffs, which derived from Sparc Target.
  /*
    // On 32bit sodium, we define a double-register 16bit register
    // class, as well. This is modeled in LLVM as a 2-vector of i16.
    addRegisterClass(MVT::v2i16, &Sodium::IntPairRegClass);

    // ...but almost all operations must be expanded, so set that as
    // the default.
    for (unsigned Op = 0; Op < ISD::BUILTIN_OP_END; ++Op) {
      setOperationAction(Op, MVT::v2i16, Expand);
    }
    // Truncating/extending stores/loads are also not supported.
    for (MVT VT : MVT::integer_fixedlen_vector_valuetypes()) {
      setLoadExtAction(ISD::SEXTLOAD, VT, MVT::v2i16, Expand);
      setLoadExtAction(ISD::ZEXTLOAD, VT, MVT::v2i16, Expand);
      setLoadExtAction(ISD::EXTLOAD, VT, MVT::v2i16, Expand);

      setLoadExtAction(ISD::SEXTLOAD, MVT::v2i16, VT, Expand);
      setLoadExtAction(ISD::ZEXTLOAD, MVT::v2i16, VT, Expand);
      setLoadExtAction(ISD::EXTLOAD, MVT::v2i16, VT, Expand);

      setTruncStoreAction(VT, MVT::v2i16, Expand);
      setTruncStoreAction(MVT::v2i16, VT, Expand);
    }
    // However, load and store *are* legal.
    setOperationAction(ISD::LOAD, MVT::v2i16, Legal);
    setOperationAction(ISD::STORE, MVT::v2i16, Legal);
    setOperationAction(ISD::EXTRACT_VECTOR_ELT, MVT::v2i16, Legal);
    setOperationAction(ISD::BUILD_VECTOR, MVT::v2i16, Legal);

    // And we need to promote i32 loads/stores into vector load/store
    setOperationAction(ISD::LOAD, MVT::i32, Custom);
    setOperationAction(ISD::STORE, MVT::i32, Custom);

    // Sadly, this doesn't work:
    //AddPromotedToType(ISD::LOAD, MVT::i32, MVT::v2i16);
    //AddPromotedToType(ISD::STORE, MVT::i32, MVT::v2i16);
  */
}

const char *SodiumTargetLowering::getTargetNodeName(unsigned Opcode) const {
#define NODE_NAME_CASE(NODE)                                                   \
  case SodiumISD::NODE:                                                        \
    return "SodiumISD::" #NODE;
  // clang-format off
  switch ((SodiumISD::NodeType)Opcode) {
  case SodiumISD::FIRST_NUMBER:
    break;
    NODE_NAME_CASE(Hi)
    NODE_NAME_CASE(AddLo)
    NODE_NAME_CASE(Call)
    NODE_NAME_CASE(Tail)
    NODE_NAME_CASE(Ret)
    NODE_NAME_CASE(ERet)
    NODE_NAME_CASE(LLA)
    NODE_NAME_CASE(BFPK)
    NODE_NAME_CASE(BFMG)
    NODE_NAME_CASE(SBFX)
    NODE_NAME_CASE(UBFX)
  }
  // clang-format on
  return nullptr;
#undef NODE_NAME_CASE
}

#include "SodiumGenCallingConv.inc"

SDValue
SodiumTargetLowering::LowerOperation(SDValue Op, SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
    case ISD::GlobalAddress:
      return LowerGlobalAddress(Op, DAG);
    case ISD::BlockAddress:
      return LowerSimpleAddress<BlockAddressSDNode>(Op, DAG);
    case ISD::ConstantPool:
      return LowerSimpleAddress<ConstantPoolSDNode>(Op, DAG);
    case ISD::JumpTable:
      return LowerSimpleAddress<JumpTableSDNode>(Op, DAG);
    case ISD::BR_JT:
      return LowerBR_JT(Op, DAG);
    case ISD::VASTART:
      return LowerVASTART(Op, DAG);
  }
  return SDValue();
}

template<typename T> void
SodiumTargetLowering::analyzeArgs(const SmallVectorImpl<T> &Args,
                                    CCState &CCInfo) const {
  unsigned NumArgs = Args.size();
  for (unsigned I = 0; I != NumArgs; ++I) {
    MVT ArgVT = Args[I].VT;
    ISD::ArgFlagsTy ArgFlags = Args[I].Flags;
    SodiumCC(I, ArgVT, ArgVT, CCValAssign::Full, ArgFlags, CCInfo);
  }
}

// Transform physical registers into virtual registers.
SDValue
SodiumTargetLowering::LowerFormalArguments(SDValue Chain,
                                           CallingConv::ID CallConv,
                                           bool IsVarArg,
                                           const SmallVectorImpl<ISD::InputArg> &Ins,
                                           const SDLoc &DL, SelectionDAG &DAG,
                                           SmallVectorImpl<SDValue> &InVals)
const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  const Function &Func = MF.getFunction();
  if (Func.hasFnAttribute("interrupt")) {
    if (!Func.arg_empty())
      report_fatal_error(
        "Functions with the interrupt attribute cannot have arguments!");
  }

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  analyzeArgs<ISD::InputArg>(Ins, CCInfo);

  for (unsigned I = 0, E = ArgLocs.size(); I != E; ++I) {
    CCValAssign &VA = ArgLocs[I];
    MVT LocVT = VA.getLocVT();
    if (VA.isRegLoc()) {
      const TargetRegisterClass *RC = getRegClassFor(LocVT);
      unsigned VReg = MF.getRegInfo().createVirtualRegister(RC);
      MF.getRegInfo().addLiveIn(VA.getLocReg(), VReg);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, VReg, LocVT);
      InVals.push_back(ArgValue);
    } else if (VA.isMemLoc()) {
      int FI = MFI.CreateFixedObject(VA.getValVT().getSizeInBits() / 8,
                                     VA.getLocMemOffset(), true);
      SDValue FIN = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
      SDValue Load = DAG.getLoad(LocVT, DL, Chain, FIN,
          MachinePointerInfo::getFixedStack(MF, FI));
      InVals.push_back(Load);
    }
  }
  return Chain;
}

SDValue
SodiumTargetLowering::LowerReturn(SDValue Chain,
                                  CallingConv::ID CallConv, bool IsVarArg,
                                  const SmallVectorImpl<ISD::OutputArg> &Outs,
                                  const SmallVectorImpl<SDValue> &OutVals,
                                  const SDLoc &DL, SelectionDAG &DAG)
const {
  MachineFunction &MF = DAG.getMachineFunction();

  SmallVector<CCValAssign, 16> RetLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RetLocs, *DAG.getContext());
  analyzeArgs<ISD::OutputArg>(Outs, CCInfo);

  SDValue Glue;
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);

  // Copy the result values into the output registers.
  for (unsigned I = 0, E = RetLocs.size(); I != E; ++I) {
    CCValAssign &VA = RetLocs[I];
    MVT LocVT = VA.getLocVT();
    assert(VA.isRegLoc());
    unsigned ArgReg = VA.getLocReg();
    Chain = DAG.getCopyToReg(Chain, DL, ArgReg, OutVals[I], Glue);
    Glue = Chain.getValue(1);
    Ops.push_back(DAG.getRegister(ArgReg, LocVT));
  }
  Ops[0] = Chain; // Update chain.
  if (Glue.getNode()) {
    Ops.push_back(Glue);
  }

  unsigned RetOpc = SodiumISD::Ret;
  // Interrupt service routines use different return instructions.
  const Function &Func = DAG.getMachineFunction().getFunction();
  if (Func.hasFnAttribute("interrupt")) {
    if (!Func.getReturnType()->isVoidTy())
      report_fatal_error(
        "Functions with the interrupt attribute must have void return type!");
    RetOpc = SodiumISD::ERet;
  }
  return DAG.getNode(RetOpc, DL, MVT::Other, Ops);
}

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
    // This generates the pattern (PseudoLLA sym), which expands to
    //   addi (auipc %pcrel_hi(sym)), %pcrel_lo(sym).
    return DAG.getNode(SodiumISD::LLA, DL, Ty, Addr);
  }

  //NonPIC Address
  SDValue AddrHi = getTargetNode(N, DL, Ty, DAG, SODIUMII::MO_HI);
  SDValue AddrLo = getTargetNode(N, DL, Ty, DAG, SODIUMII::MO_LO);
  return DAG.getNode(SodiumISD::AddLo, DL, Ty,
                     DAG.getNode(SodiumISD::Hi, DL, Ty, AddrHi),
                     AddrLo);
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

  Table = DAG.getNode(SodiumISD::LLA, DL, MVT::i16, JTI);
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
  SodiumMachineFunctionInfo *FuncInfo = MF.getInfo<SodiumMachineFunctionInfo>();

  SDLoc DL(Op);
  SDValue FI = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(),
                                 getPointerTy(MF.getDataLayout()));

  // vastart just stores the address of the VarArgsFrameIndex slot into the
  // memory location argument.
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), DL, FI, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

#include "SodiumISelLoweringOpt.h"

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

static SDValue performLogicCombine(SDNode *N,
                                   TargetLowering::DAGCombinerInfo &DCI) {
  SelectionDAG &DAG = DCI.DAG;
  if (DCI.isAfterLegalizeDAG())
    if (SDValue V = combineDeMorganOfBoolean(N, DAG))
      return V;
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


static SDValue performMULCombine(SDNode *N,
                                 TargetLowering::DAGCombinerInfo &DCI) {
  SDLoc DL(N);
  SelectionDAG &DAG = DCI.DAG;
  ConstantSDNode *C = dyn_cast<ConstantSDNode>(N->getOperand(1));
  if (!C) return SDValue();

  int64_t MulAmt = C->getSExtValue();
  unsigned ShiftAmt = llvm::countr_zero<uint64_t>(MulAmt) & (16 - 1);
  MulAmt >>= ShiftAmt;

  EVT VT = N->getValueType(0);
  SDValue X = N->getOperand(0);
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
                      Res, DAG.getConstant(ShiftAmt, DL, MVT::i16));

  // Do not add new nodes to DAG combiner worklist.
  DCI.CombineTo(N, Res, false);
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

SDValue SodiumTargetLowering::PerformDAGCombine(SDNode *N,
                                                DAGCombinerInfo &DCI) const {
  SelectionDAG &DAG = DCI.DAG;
  SDLoc dl(N);
  switch (N->getOpcode()) {
  default: break;
  case ISD::ADD:
    return performADDCombine(N, DAG);
  case ISD::SUB:
    return performSUBCombine(N, DAG);
  case ISD::XOR:
    return performXORCombine(N, DAG);
  case ISD::OR:
  case ISD::AND:
    return performLogicCombine(N, DCI);
  case ISD::MUL:
    return performMULCombine(N, DCI);
  case ISD::LOAD:
  case ISD::STORE:
    return performMemPairCombine(N, DCI);
  case ISD::SELECT_CC: {
    SDLoc DL(N);
    SDValue LHS = N->getOperand(0);
    SDValue RHS = N->getOperand(1);
    SDValue CC  = N->getOperand(2);
    SDValue TrueV  = N->getOperand(3);
    SDValue FalseV = N->getOperand(4);
    if (performCCCombine(LHS, RHS, CC, DL, DAG))
      return DAG.getNode(ISD::SELECT_CC, DL, N->getValueType(0),
                         {LHS, RHS, CC, TrueV, FalseV});
    return SDValue();
  }
  case ISD::BR_CC: {
    SDLoc DL(N);
    SDValue LHS = N->getOperand(1);
    SDValue RHS = N->getOperand(2);
    SDValue CC = N->getOperand(3);
    if (performCCCombine(LHS, RHS, CC, DL, DAG))
      return DAG.getNode(ISD::BR_CC, DL, N->getValueType(0),
                         N->getOperand(0), LHS, RHS, CC, N->getOperand(4));
    return SDValue();
  }
  }
  return SDValue();
}

/// getConstraintType - Given a constraint letter, return the type of
/// constraint it is for this target.
SodiumTargetLowering::ConstraintType
SodiumTargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    default:
      break;
    case 'd':
      return C_RegisterClass;
    case 'I':
    case 'J':
    case 'K':
      return C_Immediate;
    case 'A':
      return C_Memory;
    case 'S':
      return C_Other; // A symbolic address
    }
  }
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
SodiumTargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                   StringRef Constraint,
                                                   MVT VT) const {
  // First, see if this is a constraint that directly corresponds to a Sodium
  // register class.
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
      return std::make_pair(0U, &Sodium::IntNoX0RegClass);
    default:
      break;
    }
  }

/*
  // Clang will correctly decode the usage of register name aliases into their
  // official names. However, other frontends like `rustc` do not. This allows
  // users of these frontends to use the ABI names for registers in LLVM-style
  // register constraints.
  unsigned XRegFromAlias = StringSwitch<unsigned>(Constraint.lower())
                               .Case("{zero}", Sodium::X0)
                               .Case("{ra}", Sodium::X1)
                               .Case("{sp}", Sodium::X2)
                               .Case("{gp}", Sodium::X3)
                               .Case("{tp}", Sodium::X4)
                               .Case("{t0}", Sodium::X5)
                               .Case("{t1}", Sodium::X6)
                               .Case("{t2}", Sodium::X7)
                               .Cases("{s0}", "{fp}", Sodium::X8)
                               .Case("{s1}", Sodium::X9)
                               .Case("{a0}", Sodium::X10)
                               .Case("{a1}", Sodium::X11)
                               .Case("{a2}", Sodium::X12)
                               .Case("{a3}", Sodium::X13)
                               .Case("{a4}", Sodium::X14)
                               .Case("{a5}", Sodium::X15)
                               .Case("{a6}", Sodium::X16)
                               .Case("{a7}", Sodium::X17)
                               .Case("{s2}", Sodium::X18)
                               .Case("{s3}", Sodium::X19)
                               .Case("{s4}", Sodium::X20)
                               .Case("{s5}", Sodium::X21)
                               .Case("{s6}", Sodium::X22)
                               .Case("{s7}", Sodium::X23)
                               .Case("{s8}", Sodium::X24)
                               .Case("{s9}", Sodium::X25)
                               .Case("{s10}", Sodium::X26)
                               .Case("{s11}", Sodium::X27)
                               .Case("{t3}", Sodium::X28)
                               .Case("{t4}", Sodium::X29)
                               .Case("{t5}", Sodium::X30)
                               .Case("{t6}", Sodium::X31)
                               .Default(Sodium::NoRegister);
  if (XRegFromAlias != Sodium::NoRegister)
    return std::make_pair(XRegFromAlias, &Sodium::IntRegsRegClass);
*/

  std::pair<Register, const TargetRegisterClass *> Res =
      TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);

  return Res;
}

/// LowerAsmOperandForConstraint - Lower the specified operand into the Ops
/// vector.  If it is invalid, don't add anything to Ops.
void SodiumTargetLowering::LowerAsmOperandForConstraint(SDValue Op,
                                                        std::string &Constraint,
                                                        std::vector<SDValue>&Ops,
                                                        SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT Type = Op.getValueType();

  // Currently only support length 1 constraints.
  if (Constraint.length() == 1) {
    switch (Constraint[0]) {
    case 'I':
      // Validate & create a 12-bit signed immediate operand.
      if (auto *C = dyn_cast<ConstantSDNode>(Op)) {
        uint64_t CVal = C->getSExtValue();
        if (isInt<13>(CVal))
          Ops.push_back(
              DAG.getTargetConstant(CVal, DL, Type));
      }
      return;
    case 'J':
      // Validate & create an integer zero operand.
      if (isNullConstant(Op))
        Ops.push_back(
            DAG.getTargetConstant(0, DL, Type));
      return;
    case 'K':
      // Validate & create a 4-bit unsigned immediate operand.
      if (auto *C = dyn_cast<ConstantSDNode>(Op)) {
        uint64_t CVal = C->getZExtValue();
        if (isUInt<4>(CVal))
          Ops.push_back(
              DAG.getTargetConstant(CVal, DL, Type));
      }
      return;
    case 'S':
      if (const auto *GA = dyn_cast<GlobalAddressSDNode>(Op)) {
        Ops.push_back(DAG.getTargetGlobalAddress(GA->getGlobal(), SDLoc(Op),
                                                 GA->getValueType(0)));
      } else if (const auto *BA = dyn_cast<BlockAddressSDNode>(Op)) {
        Ops.push_back(DAG.getTargetBlockAddress(BA->getBlockAddress(),
                                                BA->getValueType(0)));
      }
      return;
    default:
      break;
    }
  }
  TargetLowering::LowerAsmOperandForConstraint(Op, Constraint, Ops, DAG);
}

bool SodiumTargetLowering::isLegalAddressingMode(const DataLayout &DL,
                                                 const AddrMode &AM, Type *Ty,
                                                 unsigned AS,
                                                 Instruction *I) const {
  // No global is ever allowed as a base.
  if (AM.BaseGV)
    return false;

  // Require a 13-bit signed offset.
  if (!isInt<13>(AM.BaseOffs))
    return false;

  switch (AM.Scale) {
  case 0:
  // "r+i" or just "i", depending on HasBaseReg.
    break;
  case 1:
  // allow "r+i".
    if (!AM.HasBaseReg)
      break;
  // disallow "r+r" or "r+r+i".
    return false;
  default:
    return false;
  }

  return true;
}
