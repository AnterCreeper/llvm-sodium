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
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsSodium.h"
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
  //setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
  //setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8, Expand);

  // Plenty of Settings.
  setTargetDAGCombine(ISD_COMBINE);
  setOperationAction(ISD_LEGAL,  MVT::i16, Legal);
  setOperationAction(ISD_EXPAND, MVT::i16, Expand);
  setOperationAction(ISD_CUSTOM, MVT::i16, Custom);

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
  setOperationAction({ISD::TRAP, ISD::DEBUGTRAP}, MVT::Other, Legal);

  // derived from Sparc Target.
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
  setOperationAction({ISD::EXTRACT_VECTOR_ELT, ISD::BUILD_VECTOR,
    ISD::LOAD, ISD::STORE}, MVT::v2i16, Legal);

  // And we need to promote i32 loads/stores into vector load/store
  setOperationAction({ISD::LOAD, ISD::STORE}, MVT::i32, Custom);

  // Sadly, this doesn't work:
  //AddPromotedToType(ISD::LOAD, MVT::i32, MVT::v2i16);
  //AddPromotedToType(ISD::STORE, MVT::i32, MVT::v2i16);

  //TODO
  //setOperationAction({ISD::ADD, ISD::SUB, ISD::MUL}, MVT::i32, Custom);

  // setup after all addRegisterClass executed
  computeRegisterProperties(Subtarget.getRegisterInfo());
}

const char *SodiumTargetLowering::getTargetNodeName(unsigned Opcode) const {
#define NODE_NAME_CASE(NODE)                                                   \
  case SodiumISD::NODE:                                                        \
    return "SodiumISD::" #NODE;
  // clang-format off
  switch ((SodiumISD::NodeType)Opcode) {
  case SodiumISD::FIRST_NUMBER:
    break;
    NODE_NAME_CASE(LI)
    NODE_NAME_CASE(LA)
    NODE_NAME_CASE(Call)
    NODE_NAME_CASE(Tail)
    NODE_NAME_CASE(Ret)
    NODE_NAME_CASE(ERet)
    NODE_NAME_CASE(TBE)
  }
  // clang-format on
  return nullptr;
#undef NODE_NAME_CASE
}

#include "SodiumGenCallingConv.inc"

// Lower Methods
#include "SodiumLowerFunc.h"

//illegal ins expand
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
  case ISD::SELECT:
    return LowerSelect(Op, DAG);
  case ISD::SMUL_LOHI:
    return LowerMUL_LOHI(Op, DAG, true);
  case ISD::UMUL_LOHI:
    return LowerMUL_LOHI(Op, DAG, false);
  case ISD::STORE: {
    SDNode *N = Op.getNode();
    SDLoc DL(N);
    StoreSDNode *St = cast<StoreSDNode>(N);
    if (St->getMemoryVT() != MVT::i32)
      break;

    // Custom handling for i32 stores: turn it into a bitcast and a
    // v2i16 store.
    SDValue Op0 = DAG.getBitcast(MVT::v2i16, St->getValue());
    SDValue Chain = DAG.getStore(St->getChain(), DL, Op0,
                                 St->getBasePtr(), St->getPointerInfo(),
                                 St->getOriginalAlign(), St->getMemOperand()->getFlags(),
                                 St->getAAInfo());
    return Chain;
  }
  }
  return SDValue();
}

#define GETBITS(x, n, m) ((x >> n) & ((1 << (m - n + 1)) - 1))

//illegal outs expand
void SodiumTargetLowering::ReplaceNodeResults(SDNode *N,
                                              SmallVectorImpl<SDValue>& Results,
                                              SelectionDAG &DAG) const {
  SDLoc DL(N);
  switch (N->getOpcode()) {
  default:
    llvm_unreachable("Do not know how to custom type legalize this operation!");
  case ISD::LOAD: {
    LoadSDNode *Ld = cast<LoadSDNode>(N);
    if (Ld->getValueType(0) != MVT::i32 || Ld->getMemoryVT() != MVT::i32)
      break;

    // Custom handling only for i32: turn i32 load into a v2i16 load,
    // and a bitcast.
    SDValue Op0 = DAG.getExtLoad(Ld->getExtensionType(), DL, MVT::v2i16, Ld->getChain(),
                                 Ld->getBasePtr(), Ld->getPointerInfo(), MVT::v2i16,
                                 Ld->getOriginalAlign(), Ld->getMemOperand()->getFlags(),
                                 Ld->getAAInfo());
    SDValue Chain = DAG.getBitcast(MVT::i32, Op0);
    Results.push_back(Chain);
    Results.push_back(Op0.getValue(1));
    break;
  }
  case ISD::ADD:
  case ISD::SUB: {
    //TODO
    break;
  }
  }
}

template<typename T> static void
analyzeArgs(const SmallVectorImpl<T> &Args, CCState &CCInfo) {
  unsigned NumArgs = Args.size();
  for (unsigned I = 0; I != NumArgs; ++I) {
    MVT ArgVT = Args[I].VT;
    ISD::ArgFlagsTy ArgFlags = Args[I].Flags;
    SodiumCC(I, ArgVT, ArgVT, CCValAssign::Full, ArgFlags, CCInfo);
  }
}

static const MCPhysReg ArgGPRs[] = {  //a0-a7
  Sodium::X10, Sodium::X11, Sodium::X12, Sodium::X13,
  Sodium::X14, Sodium::X15, Sodium::X16, Sodium::X17
};

/// writeVarArgRegs - Write variable function arguments passed in registers
/// to the stack. Also create a stack frame object for the first variable
/// argument.
void SodiumTargetLowering::writeVarArgRegs(std::vector<SDValue> &OutChains,
                                         SDValue Chain, const SDLoc &DL,
                                         SelectionDAG &DAG,
                                         CCState &CCInfo) const {
  ArrayRef<MCPhysReg> ArgRegs = ArrayRef(ArgGPRs);
  unsigned Idx = CCInfo.getFirstUnallocated(ArgRegs);
  const TargetRegisterClass *RC = &Sodium::IntRegsRegClass;
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();
  SodiumMachineFunctionInfo *SodiumFI = MF.getInfo<SodiumMachineFunctionInfo>();

  // Offset of the first variable argument from stack pointer, and size of
  // the vararg save area. For now, the varargs save area is either zero or
  // large enough to hold a0-a7.
  int VaArgOffset, VarArgsSaveSize;

  // If all registers are allocated, then all varargs must be passed on the
  // stack and we don't need to save any argregs.
  MVT RegTy = MVT::i16;
  unsigned RegSizeInBytes = 2;
  if (ArgRegs.size() == Idx) {
    VaArgOffset = CCInfo.getStackSize();
    VarArgsSaveSize = 0;
  } else {
    VarArgsSaveSize = RegSizeInBytes * (ArgRegs.size() - Idx);
    VaArgOffset = -VarArgsSaveSize;
  }

  // Record the frame index of the first variable argument
  // which is a value necessary to VASTART.
  int FI = MFI.CreateFixedObject(RegSizeInBytes, VaArgOffset, true);
  SodiumFI->setVarArgsFrameIndex(FI);

  // Copy the integer registers that have not been used for argument passing
  // to the argument register save area. For O32, the save area is allocated
  // in the caller's stack frame, while for N32/64, it is allocated in the
  // callee's stack frame.
  for (unsigned I = Idx; I < ArgRegs.size();
       ++I, VaArgOffset += RegSizeInBytes) {
    const Register Reg = RegInfo.createVirtualRegister(RC);
    RegInfo.addLiveIn(ArgRegs[I], Reg);
    SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, Reg, RegTy);
    FI = MFI.CreateFixedObject(RegSizeInBytes, VaArgOffset, true);
    SDValue PtrOff = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
    SDValue Store =
        DAG.getStore(Chain, DL, ArgValue, PtrOff,
                     MachinePointerInfo::getFixedStack(MF, FI));
    cast<StoreSDNode>(Store.getNode())->getMemOperand()->setValue(
        (Value *)nullptr);
    OutChains.push_back(Store);
  }
  //FIXME
  //SodiumFI->setVarArgsSaveSize(VarArgsSaveSize);
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

  // Used with vargs to acumulate store chains.
  std::vector<SDValue> OutChains;

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
      OutChains.push_back(Load.getValue(1));
    }
  }

  if (IsVarArg)
    writeVarArgRegs(OutChains, Chain, DL, DAG, CCInfo);

  // All stores are grouped in one node to allow the matching between
  // the size of Ins and InVals. This only happens when on varg functions
  if (!OutChains.empty()) {
    OutChains.push_back(Chain);
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, OutChains);
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

// DAGCombine Methods
#include "SodiumISelLoweringOpt.h"

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
  case ISD::MUL:
    return performMULCombine(N, DCI);
  case ISD::XOR:
    return performXORCombine(N, DAG);
  case ISD::OR:
  case ISD::AND:
    return performLogicCombine(N, DCI);
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
      if (VT == MVT::v2i16)
        return std::make_pair(0U, &Sodium::PairNoX0RegClass);
      else
        return std::make_pair(0U, &Sodium::RegsNoX0RegClass);
    default:
      break;
    }
  }

  // Clang will correctly decode the usage of register name aliases into their
  // official names. However, other frontends like `rustc` do not. This allows
  // users of these frontends to use the ABI names for registers in LLVM-style
  // register constraints.
  //TODO

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
