//===-- SodiumInstrInfo.td - Target Description for Sodium Target ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the Sodium instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Instruction format superclass
//===----------------------------------------------------------------------===//

include "SodiumInstrFormats.td"

//===----------------------------------------------------------------------===//
// Custom ISD SDNode Definition, at SodiumISelLowering.h
//===----------------------------------------------------------------------===//
// SDTypeProfile<numresults, numoperands, constraints>
def SDTCall : SDTypeProfile<0, -1, [SDTCisVT<0, XLenVT>]>;
def SDTCallSeq_End : SDCallSeqEnd<[SDTCisVT<0, i16>, SDTCisVT<1, i16>]>;
def SDTCallSeq_Start : SDCallSeqStart<[SDTCisVT<0, i16>, SDTCisVT<1, i16>]>;

// Custom nodes
def sodium_hi : SDNode<"SodiumISD::Hi", SDTIntUnaryOp>;
def sodium_add_lo : SDNode<"SodiumISD::AddLo", SDTIntBinOp>;
def sodium_ret : SDNode<"SodiumISD::Ret", SDTNone,
  [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def sodium_eret : SDNode<"SodiumISD::ERet", SDTNone,
  [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def sodium_call : SDNode<"SodiumISD::Call", SDTCall,
  [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic]>;
def sodium_tail : SDNode<"SodiumISD::Tail", SDTCall,
  [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic]>;
def sodium_lla : SDNode<"SodiumISD::LLA", SDTIntUnaryOp>;

// Target-independent nodes, but with target-specific formats.
def callseq_end : SDNode<"ISD::CALLSEQ_END", SDTCallSeq_End,
  [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
def callseq_start : SDNode<"ISD::CALLSEQ_START", SDTCallSeq_Start,
  [SDNPHasChain, SDNPOutGlue]>;

//===----------------------------------------------------------------------===//
// Register Operand Definition
//===----------------------------------------------------------------------===//
def MemRegs : RegisterOperand<IntRegs>{
  let OperandType = "OPERAND_MEMORY";
}

//===----------------------------------------------------------------------===//
// Immediate Operand Class Templates
//===----------------------------------------------------------------------===//
class ImmAsmOperand<string prefix, int width, string suffix> : AsmOperandClass {
  let Name = prefix # "Imm" # width # suffix;
  let RenderMethod = "addImmOperands";
}

class SImmAsmOperand<int width, string suffix = "">
  : ImmAsmOperand<"S", width, suffix> {
}

class UImmAsmOperand<int width, string suffix = "">
  : ImmAsmOperand<"U", width, suffix> {
}

//===----------------------------------------------------------------------===//
// Immediate Operand Definition
//===----------------------------------------------------------------------===//
def uimm4 : Operand<i16>,
    ImmLeaf<i16, [{return isInt<4>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<4>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<4>";
}

def simm13 : Operand<i16>,
    ImmLeaf<i16, [{return isInt<13>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<13>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeSImmOperand<13>";
}

def uimm3_plus1 : Operand<i16>,
    ImmLeaf<i16, [{return isUInt<3>(Imm - 1);}]> {
  let ParserMatchClass = UImmAsmOperand<3, "plus1">;
  let EncoderMethod = "getImmOpValueSub1";
  let DecoderMethod = "decodeUImmOperandAndPlus1<3>";
}

//Load Relative Upper Immediate
class UImm20Operand<ValueType VT> : Operand<VT>,
    ImmLeaf<VT, [{return isInt<20>(Imm);}]> {
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<20>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<20>(Imm);
    return MCOp.isBareSymbolRef();
  }];
}

def uimm20_lui16 : UImm20Operand<i16> {
  let ParserMatchClass = UImmAsmOperand<20, "LUI16">;
}
def uimm20_lui32 : UImm20Operand<i32> {
  let ParserMatchClass = UImmAsmOperand<20, "LUI32">;
}
def uimm20_auipc : UImm20Operand<XLenVT> {
  let ParserMatchClass = UImmAsmOperand<20, "AUIPC">;
}

// A 26-bit signed immediate where the least significant bit is zero.
def SImm26Lsb0BranchAsmOperand : SImmAsmOperand<26, "branch"> {
  let ParserMethod = "parseBranchOffset";
}

def simm25_jump : Operand<OtherVT> {
  let ParserMatchClass = SImm26Lsb0BranchAsmOperand;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr<1>";
  let DecoderMethod = "decodeSImmOperandAndLsl1<25>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<25, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

// A 21-bit signed immediate where the least significant bit is zero.
def simm20_brcc : Operand<OtherVT> {
  let ParserMatchClass = SImmAsmOperand<21, "brcc">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr<1>";
  let DecoderMethod = "decodeSImmOperandAndLsl1<20>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<20, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

// A 21-bit signed immediate where the least significant bit is zero.
def simm20_brind : Operand<XLenVT>,
    ImmLeaf<XLenVT, [{return isInt<21>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<21>;
  let EncoderMethod = "getImmOpValueAsr<1>";
  let DecoderMethod = "decodeSImmOperandAndLsl1<20>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<20, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
}

def CallSymbol : AsmOperandClass {
  let Name = "CallSymbol";
  let RenderMethod = "addImmOperands";
  let ParserMethod = "parseCallSymbol";
}

def JumpSymbol : AsmOperandClass {
  let Name = "JumpSymbol";
  let RenderMethod = "addImmOperands";
  let ParserMethod = "parseJumpSymbol";
}

def BareSymbol : AsmOperandClass {
  let Name = "BareSymbol";
  let RenderMethod = "addImmOperands";
  let ParserMethod = "parseImmediate";
}

// A bare symbol used in call/tail only.
def call_symbol : Operand<XLenVT> {
  let ParserMatchClass = CallSymbol;
}

// A bare symbol used in pseudo jump only.
def jump_symbol : Operand<XLenVT> {
  let ParserMatchClass = JumpSymbol;
}

// A bare symbol used in "PseudoL*A" instructions.
def bare_symbol : Operand<XLenVT> {
  let ParserMatchClass = BareSymbol;
}

//===----------------------------------------------------------------------===//
// Instruction Class Templates
//===----------------------------------------------------------------------===//
class Load_ri<bits<3> func3, bits<2> tag2, string opcodestr>
    : FMT_LS<func3, tag2, (outs IntRegs:$rs), (ins MemRegs:$rb, simm13:$imm13),
              opcodestr, "$rs, ${imm13}(${rb})">,
      Sched<[WriteLoad, ReadMemBase]>;
class Load_di<bits<3> func3, bits<2> tag2, string opcodestr>
    : FMT_LS<func3, tag2, (outs IntPair:$rs), (ins MemRegs:$rb, simm13:$imm13),
              opcodestr, "$rs, ${imm13}(${rb})">,
      Sched<[WriteLoad, ReadMemBase]>;

class Store_rri<bits<3> func3, bits<2> tag2, string opcodestr>
    : FMT_LS<func3, tag2, (outs), (ins IntRegs:$rs, MemRegs:$rb, simm13:$imm13),
              opcodestr, "$rs, ${imm13}(${rb})">,
      Sched<[WriteStore, ReadStoreData, ReadMemBase]>;
class Store_dri<bits<3> func3, bits<2> tag2, string opcodestr>
    : FMT_LS<func3, tag2, (outs), (ins IntPair:$rs, MemRegs:$rb, simm13:$imm13),
              opcodestr, "$rs, ${imm13}(${rb})">,
      Sched<[WriteStore, ReadStoreData, ReadMemBase]>;

class Cache_ri<bits<3> func3, bits<2> tag2, string opcodestr>
    : FMT_LS<func3, tag2, (outs), (ins MemRegs:$rb, simm13:$imm13),
              opcodestr, "${imm13}(${rb})">,
      Sched<[ReadMemBase]> {
  let rs = 0b00000;
}

class ALU_ri<bits<3> func3, bits<3> tag3, string opcodestr>
    : FMT_I<func3, tag3, (outs IntRegs:$rd), (ins IntRegs:$rs, simm13:$imm13),
              opcodestr, "$rd, $rs, $imm13">,
      Sched<[WriteIALU, ReadIALU]>;

class BFM_rii<bits<3> func3, bits<3> tag3, bits<1> ifsigned, string opcodestr>
    : FMT_II<func3, tag3, ifsigned, (outs IntRegs:$rd), (ins IntRegs:$rs, uimm4:$lsb, uimm4:$len),
              opcodestr, "$rd, $rs, $lsb, $len">,
      Sched<[WriteIALU, ReadIALU]>;

class ALUd_di<bits<3> func3, bits<3> tag3, string opcodestr>
    : FMT_I<func3, tag3, (outs IntPair:$rd), (ins IntPair:$rs, simm13:$imm13),
              opcodestr, "$rd, $rs, $imm13">,
      Sched<[WriteIALU, ReadIALU]>;

class ALU_rr<bits<3> func3, bits<3> tag3, bits<7> flag7, string opcodestr>
    : FMT_R<func3, tag3, flag7, (outs IntRegs:$rd), (ins IntRegs:$rs1, IntRegs:$rs2),
              opcodestr, "$rd, $rs1, $rs2">,
      Sched<[WriteIALU, ReadIALU, ReadIALU]>;

class ALU_rri<bits<3> func3, bits<3> tag3, string opcodestr>
    : FMT_RRI<func3, tag3, (outs IntRegs:$rd), (ins IntRegs:$rs1, IntRegs:$rs2, uimm4:$flag7),
              opcodestr, "$rd, $rs1, $rs2, $flag7">,
      Sched<[WriteIALU, ReadIALU, ReadIALU]>;

class SHADD_rri<bits<3> func3, bits<3> tag3, string opcodestr>
    : FMT_RRI<func3, tag3, (outs IntRegs:$rd), (ins IntRegs:$rs1, IntRegs:$rs2, uimm3_plus1:$flag7),
              opcodestr, "$rd, $rs1, $rs2, $flag7">,
      Sched<[WriteIALU, ReadIALU, ReadIALU]>;

class ALUd_rr<bits<3> func3, bits<3> tag3, bits<7> flag7, string opcodestr>
    : FMT_R<func3, tag3, flag7, (outs IntPair:$rd), (ins IntRegs:$rs1, IntRegs:$rs2),
              opcodestr, "$rd, $rs1, $rs2">,
      Sched<[WriteIALU, ReadIALU, ReadIALU]>;

class ALUd_dr<bits<3> func3, bits<3> tag3, bits<7> flag7, string opcodestr>
    : FMT_R<func3, tag3, flag7, (outs IntPair:$rd), (ins IntPair:$rs1, IntRegs:$rs2),
              opcodestr, "$rd, $rs1, $rs2">,
      Sched<[WriteIALU, ReadIALU, ReadIALU]>;

class ALU_r<bits<3> func3, bits<3> tag3, bits<7> flag7, string opcodestr>
    : FMT_R<func3, tag3, flag7, (outs IntRegs:$rd), (ins IntRegs:$rs1),
              opcodestr, "$rd, $rs1">,
      Sched<[WriteIALU, ReadIALU]> {
  let rs2 = 0b00000;
}

class CMOV_rr<bits<3> func3, bits<3> tag3, bits<7> flag7, string opcodestr>
    : FMT_R<func3, tag3, flag7, (outs IntRegs:$rd), (ins IntRegs:$T, IntRegs:$rs1, IntRegs:$rs2),
              opcodestr, "$rd, $rs1, $rs2">,
      Sched<[WriteIALU, ReadIALU, ReadIALU]> {
  let Constraints = "$T = $rd";
}

class CondB_ri<bits<3> func3, string opcodestr>
    : FMT_B<func3, (outs),
                   (ins IntRegs:$rs, simm20_brcc:$imm20),
                   opcodestr, "$rs, $imm20">,
      Sched<[WriteBranch, ReadBranch]> {
  let isBranch = 1;
  let isTerminator = 1;
}

class HINT<bits<3> tag3, string asmstr>
    : FMT_HT<0b000, tag3, 0, (outs), (ins), asmstr, "">;

//===----------------------------------------------------------------------===//
// Instructions common
//===----------------------------------------------------------------------===//
//Load Store
let IsSignExt = 1 in {
def LW : Load_ri<0b001, 0b00, "lw">;
def LB : Load_ri<0b001, 0b10, "lb">;
def LBU : Load_ri<0b001, 0b11, "lbu">;
}
def SW : Store_rri<0b000, 0b00, "sw">;
def SB : Store_rri<0b000, 0b10, "sb">;
let IsSignExt = 1 in {
def LD : Load_di<0b001, 0b01, "ld">;
def SD : Store_dri<0b000, 0b01, "sd">;
}

def DCZERO  : Cache_ri<0b100, 0b01, "dc.zero">;
def DCINVD  : Cache_ri<0b110, 0b01, "dc.invd">;
def DCFLUSH : Cache_ri<0b101, 0b01, "dc.flush">;
def DCCLEAN : Cache_ri<0b111, 0b01, "dc.clean">;

//Load Relative Address
let IsSignExt = 1 in {
def LUI :   FMT_LRA<0b001, (outs IntRegs:$rd), (ins uimm20_lui16:$imm20),
            "lui",    "$rd, $imm20">, Sched<[WriteIALU]>;
def LUI32 : FMT_LRA<0b011, (outs IntPair:$rd), (ins uimm20_lui32:$imm20),
            "lui.32", "$rd, $imm20">, Sched<[WriteIALU]>;
def AUIPC : FMT_LRA<0b000, (outs IntRegs:$rd), (ins uimm20_auipc:$imm20),
            "auipc",  "$rd, $imm20">, Sched<[WriteIALU]>;
}

//Read CSR
class CSRRr_ri<bits<2> tag2, string opcodestr, RegisterClass RC>
  : FMT_SR<0b100, tag2, (outs RC:$rs), (ins MemRegs:$rb, simm13:$imm13),
              opcodestr, "$rs, ${imm13}(${rb})">,
      Sched<[ReadCSR, ReadMemBase]>;

//Write CSR
class CSRW_rri<bits<2> tag2, string opcodestr, RegisterClass RC>
  : FMT_SR<0b010, tag2, (outs), (ins RC:$rs, MemRegs:$rb, simm13:$imm13),
              opcodestr, "$rs, ${imm13}(${rb})">,
      Sched<[WriteCSR, ReadMemBase]>;

//Exchange CSR
class CSRXr_rri<bits<2> tag2, string opcodestr, RegisterClass RC>
  : FMT_SR<0b000, tag2, (outs RC:$T), (ins RC:$rs, MemRegs:$rb, simm13:$imm13),
              opcodestr, "$rs, ${imm13}(${rb})">,
      Sched<[ReadCSR, WriteCSR, ReadMemBase]> {
  let Constraints = "$T = $rs";
}

def RCSR : CSRRr_ri<0b01, "rcsr", IntRegs>;
def RCSR32 : CSRRr_ri<0b10, "rcsr.32", IntPair>;
def RCSRHI : CSRRr_ri<0b11, "rcsr.hi", IntRegs>;

def WCSR : CSRW_rri<0b01, "wcsr", IntRegs>;
def WCSR32 : CSRW_rri<0b10, "wcsr.32", IntPair>;
def WCSRHI : CSRW_rri<0b11, "wcsr.hi", IntRegs>;

def XCSR : CSRXr_rri<0b01, "xcsr", IntRegs>;
def XCSR32 : CSRXr_rri<0b10, "xcsr.32", IntPair>;
def XCSRHI : CSRXr_rri<0b11, "xcsr.hi", IntRegs>;

//Arith
def ADD : ALU_rr<0b000, 0b000, 0, "add">;
def SUB : ALU_rr<0b000, 0b011, 0, "sub">;
let IsSignExt = 1 in {
def SLT : ALU_rr<0b000, 0b100, 0, "slt">;
def SLTU : ALU_rr<0b000, 0b110, 0, "sltu">;
}
def ADDI : ALU_ri<0b000, 0b000, "addi">;
let IsSignExt = 1 in {
  def SLTI : ALU_ri<0b000, 0b100, "slti">;
  def SLTUI : ALU_ri<0b000, 0b110, "sltui">;
}
def ADD32 : ALUd_dr<0b000, 0b010, 0, "add.32">;
def SUB32 : ALUd_dr<0b000, 0b001, 0, "sub.32">;
def ADDI32 : ALUd_di<0b000, 0b010, "addi.32">;

//Shift
def SLL : ALU_rr<0b010, 0b000, 0, "sll">;
def SRL : ALU_rr<0b010, 0b010, 0, "srl">;
def SRA : ALU_rr<0b010, 0b100, 0, "sra">;
def ROL : ALU_rr<0b010, 0b110, 0, "rol">;
def SLLI : ALU_ri<0b010, 0b000, "slli">;
def SRLI : ALU_ri<0b010, 0b010, "srli">;
def SRAI : ALU_ri<0b010, 0b100, "srai">;
def ROLI : ALU_ri<0b010, 0b110, "roli">;
def SHADD : SHADD_rri<0b010, 0b111, "shadd">;

//Logical
def OR : ALU_rr<0b001, 0b000, 0, "or">;
def AND : ALU_rr<0b001, 0b100, 0, "and">;
def XOR : ALU_rr<0b001, 0b010, 0, "xor">;
def ORI : ALU_ri<0b001, 0b000, "ori">;
def ANDI : ALU_ri<0b001, 0b100, "andi">;
def XORI : ALU_ri<0b001, 0b010, "xori">;
def ORN : ALU_rr<0b001, 0b001, 0, "orn">;
def ANDN : ALU_rr<0b001, 0b101, 0, "andn">;
def XNOR : ALU_rr<0b001, 0b011, 0, "xnor">;

//Multiply
def MUL : ALU_rr<0b011, 0b000, 0, "mul">;
def MULH : ALU_rr<0b011, 0b100, 0, "mulh">;
def MULHU : ALU_rr<0b011, 0b110, 0, "mulhu">;
def MULI : ALU_ri<0b011, 0b000, "muli">;
def MULHI : ALU_ri<0b011, 0b100, "mulhi">;
//TODO:
//mul.d, mulu.d(smul_lohi, umul_lohi)
//def MULHUS : ALU_rr<0b011, 0b111, 0, "mulhus">;
//def MULHUSI : ALU_ri<0b011, 0b110, "mulhusi">;

//Bitmanipulation
def REV : ALU_r<0b101, 0b011, 0, "rev">;
def CLZ : ALU_r<0b101, 0b101, 0, "clz">;
def MIN : ALU_rr<0b101, 0b001, 0, "min">;
def MAX : ALU_rr<0b101, 0b001, 2, "max">;
def MINU : ALU_rr<0b101, 0b001, 1, "minu">;
def MAXU : ALU_rr<0b101, 0b001, 3, "maxu">;
def PACK : ALU_rri<0b101, 0b111, "pack">;
def UBFI : BFM_rii<0b101, 0b000, 0, "ubfi">;
def SBFI : BFM_rii<0b101, 0b000, 1, "sbfi">;
def UBFX : BFM_rii<0b101, 0b010, 0, "ubfx">;
def SBFX : BFM_rii<0b101, 0b010, 1, "sbfx">;

//Cond Move
def MOVZ : CMOV_rr<0b100, 0b000, 0, "movz">;
def MOVN : CMOV_rr<0b100, 0b001, 0, "movn">;

//System Hint
def NOP : HINT<0b000, "nop">;
def WFI : HINT<0b001, "wfi">;

//System Call
def ECALL : FMT_HT<0b001, 1, 0, (outs), (ins), "ecall", "">,  Sched<[WriteBranch]>;
def EBREAK : FMT_HT<0b001, 2, 0, (outs), (ins), "ebreak", "">, Sched<[WriteBranch]>;

def ADJCALLSTACKUP : Pseudo<(outs), (ins i16imm:$amt1, i16imm:$amt2),
                     "!ADJCALLSTACKUP", "$amt1",
                     [(callseq_end timm:$amt1, timm:$amt2)]>;
def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i16imm:$amt1, i16imm:$amt2),
                       "!ADJCALLSTACKDOWN", "$amt1, $amt2",
                       [(callseq_start timm:$amt1, timm:$amt2)]>;

def B : FMT_Ji<0b000,   (outs), (ins simm25_jump:$imm25),
  "b", "$imm25", []>,         Sched<[WriteBranch]>;
def BL : FMT_Ji<0b001,  (outs), (ins simm25_jump:$imm25),
  "bl", "$imm25", []>,        Sched<[WriteIALU, WriteBranch]>;
def BR : FMT_Jr<0b100,  (outs), (ins IntRegs:$rb, simm20_brind:$imm20),
  "br", "$rb, $imm20", []>,   Sched<[WriteBranch, ReadBranch]>;
def BLR : FMT_Jr<0b101, (outs), (ins IntRegs:$rb, simm20_brind:$imm20),
  "blr", "$rb, $imm20", []>,  Sched<[WriteIALU, WriteBranch, ReadBranch]>;

let isBarrier = 1, isReturn = 1, isTerminator = 1 in {
def RET : FMT_J<0b010, 0b00010, (outs), (ins),
  "ret", "",  [(sodium_ret)]>,  Sched<[WriteBranch, ReadBranch]>;
def ERET : FMT_J<0b110, 0b00000, (outs), (ins),
  "eret", "", [(sodium_eret)]>, Sched<[WriteBranch, ReadBranch]>;
}

//===----------------------------------------------------------------------===//
// Instructions related to Flags, which have no fixups.
//===----------------------------------------------------------------------===//
let isBarrier = 1, isBranch = 1, isTerminator = 1 in
def PseudoBR : Pseudo<(outs), (ins simm25_jump:$imm25), "", "", [(br bb:$imm25)]>,
                PseudoInstExpansion<(B simm25_jump:$imm25)>;  //jump flag => B offset(flag)

let isBarrier = 1, isBranch = 1, isIndirectBranch = 1, isTerminator = 1 in
def PseudoBRIND : Pseudo<(outs), (ins IntRegs:$rb, simm20_brind:$imm20), "", "", []>,
                PseudoInstExpansion<(BR IntRegs:$rb, simm20_brind:$imm20)>;
                                                              //brind jt  => BR $rb(flag), offset(flag)

def CBZ  : CondB_ri<0b001, "cbz">;
def CBNZ : CondB_ri<0b101, "cbnz">;
def CBGE : CondB_ri<0b011, "cbge">;
def CBLT : CondB_ri<0b110, "cblt">;
def CBGT : CondB_ri<0b010, "cbgt">;
def CBLE : CondB_ri<0b111, "cble">;

//===----------------------------------------------------------------------===//
// Instructions related to Symbols, which need expand and fixup.
//===----------------------------------------------------------------------===//
// Expanded(expand at emit level, and share one fixup) at XXXMCCodeEmitter.cpp
let isBarrier = 1, isBranch = 1, isIndirectBranch = 1, isTerminator = 1, Size = 8 in
def PseudoJump : Pseudo<(outs), (ins jump_symbol:$dst),       //which is rare in use
                "jmp", "$dst", []>,
                Sched<[WriteBranch, ReadBranch]>;             //brind sym  => BR (AUIPC %hi(sym)), %lo(sym),   relax to BR X0, offset(sym)
                                                              // formed by insertIndirectBranch, expanded by expandFunctionCall
let isCall = 1, Size = 8 in
def PseudoCall : Pseudo<(outs), (ins call_symbol:$dst),
                "call", "$dst", []>,
                Sched<[WriteIALU, WriteBranch]>;              //call sym   => BLR (AUIPC %hi(sym)), %lo(sym),  relax to BL offset(sym)
let isCall = 1 in
def PseudoCallIndirect : Pseudo<(outs), (ins IntRegs:$rb), "", "",
                [(sodium_call IntRegs:$rb)]>,
                PseudoInstExpansion<(BLR IntRegs:$rb, 0)>;    //call (sym) => BLR $rb(sym), 0

let isCall = 1, isTerminator = 1, isReturn = 1, isBarrier = 1, Size = 8 in
def PseudoTail : Pseudo<(outs), (ins call_symbol:$dst),
                "tail", "$dst", []>,
                 Sched<[WriteIALU, WriteBranch, ReadBranch]>; //tail sym   => BLR (AUIPC %hi(sym)), %lo(sym),  relax to BL offset(sym)

let isCall = 1, isTerminator = 1, isReturn = 1, isBarrier = 1 in
def PseudoTailIndirect : Pseudo<(outs), (ins IntRegs:$rb), "", "",
                [(sodium_tail IntRegs:$rb)]>,
                PseudoInstExpansion<(BLR IntRegs:$rb, 0)>;    //tail (sym) => BLR $rb(sym), 0

// Expanded at XXXExpandPseudoInsts.cpp
let Size = 8 in {
def PseudoLLA : Pseudo<(outs IntRegs:$dst), (ins bare_symbol:$src),
                "lla", "$dst, $src", []>;                     //lla sym    => ADDI (AUIPC %hi(sym)), %lo(sym)
}

//===----------------------------------------------------------------------===//
// Codegen patterns
//
// Naming convention: For 'generic' pattern classes, we use the naming
// convention PatTy1Ty2. For pattern classes which offer a more complex
// expansion, prefix the class name, e.g. ccBrPats.
//===----------------------------------------------------------------------===//
//Branch Indirect Pattern
def : Pat<(brind IntRegs:$rb), (PseudoBRIND IntRegs:$rb, 0)>;
def : Pat<(brind (add IntRegs:$rb, simm20_brind:$imm20)),
  (PseudoBRIND IntRegs:$rb, simm20_brind:$imm20)>;

//Call&Tail Pattern
def : Pat<(sodium_call tglobaladdr:$func), (PseudoCall tglobaladdr:$func)>;
def : Pat<(sodium_call texternalsym:$func), (PseudoCall texternalsym:$func)>;
def : Pat<(sodium_tail (iPTR tglobaladdr:$dst)), (PseudoTail tglobaladdr:$dst)>;
def : Pat<(sodium_tail (iPTR texternalsym:$dst)), (PseudoTail texternalsym:$dst)>;

//Load Symbol
def : Pat<(sodium_hi tjumptable:$in), (LUI tjumptable:$in)>;
def : Pat<(sodium_hi tconstpool:$in), (LUI tconstpool:$in)>;
def : Pat<(sodium_hi tglobaladdr:$in), (LUI tglobaladdr:$in)>;
def : Pat<(sodium_hi tblockaddress:$in), (LUI tblockaddress:$in)>;
def : Pat<(sodium_add_lo IntRegs:$hi, tglobaladdr:$lo),
          (ADDI IntRegs:$hi, tglobaladdr:$lo)>;
def : Pat<(sodium_add_lo IntRegs:$hi, tblockaddress:$lo),
          (ADDI IntRegs:$hi, tblockaddress:$lo)>;
def : Pat<(sodium_add_lo IntRegs:$hi, tjumptable:$lo),
          (ADDI IntRegs:$hi, tjumptable:$lo)>;
def : Pat<(sodium_add_lo IntRegs:$hi, tconstpool:$lo),
          (ADDI IntRegs:$hi, tconstpool:$lo)>;

def : Pat<(sodium_lla tjumptable:$in), (PseudoLLA tjumptable:$in)>;
def : Pat<(sodium_lla tconstpool:$in), (PseudoLLA tconstpool:$in)>;
def : Pat<(sodium_lla tglobaladdr:$in), (PseudoLLA tglobaladdr:$in)>;
def : Pat<(sodium_lla tblockaddress:$in), (PseudoLLA tblockaddress:$in)>;

//Cond Move
def : Pat<(select IntRegs:$rs1, IntRegs:$T, IntRegs:$rs2),
  (MOVZ IntRegs:$T, IntRegs:$rs1, IntRegs:$rs2)>;
def : Pat<(select IntRegs:$rs1, IntRegs:$rs2, IntRegs:$T),
  (MOVN IntRegs:$T, IntRegs:$rs1, IntRegs:$rs2)>;

def : Pat<(select (i16 (seteq IntRegs:$rs1, 0)), IntRegs:$T, IntRegs:$rs2),
  (MOVN IntRegs:$T, IntRegs:$rs1, IntRegs:$rs2)>;
def : Pat<(select (i16 (seteq IntRegs:$rs1, 0)), IntRegs:$rs2, IntRegs:$T),
  (MOVZ IntRegs:$T, IntRegs:$rs1, IntRegs:$rs2)>;

def : Pat<(select (i16 (setne IntRegs:$rs1, 0)), IntRegs:$T, IntRegs:$rs2),
  (MOVZ IntRegs:$T, IntRegs:$rs1, IntRegs:$rs2)>;
def : Pat<(select (i16 (setne IntRegs:$rs1, 0)), IntRegs:$rs2, IntRegs:$T),
  (MOVN IntRegs:$T, IntRegs:$rs1, IntRegs:$rs2)>;

def : Pat<(select (i16 (seteq IntRegs:$rs1, 0)), 0, IntRegs:$T),
  (MOVZ IntRegs:$T, IntRegs:$rs1, X0)>;
def : Pat<(select (i16 (setne IntRegs:$rs1, 0)), 0, IntRegs:$T),
  (MOVN IntRegs:$T, IntRegs:$rs1, X0)>;

multiclass ccMvPats<PatFrag CondOp, SodiumInst CondInst, SodiumInst A, SodiumInst B> {
def : Pat<(select (i16 (CondOp IntRegs:$rs1, IntRegs:$rs2)), IntRegs:$rs3, IntRegs:$T),
  (A IntRegs:$T, (CondInst IntRegs:$rs1, IntRegs:$rs2), IntRegs:$rs3)>;
def : Pat<(select (i16 (CondOp IntRegs:$rs1, IntRegs:$rs2)), IntRegs:$T, IntRegs:$rs3),
  (B IntRegs:$T, (CondInst IntRegs:$rs1, IntRegs:$rs2), IntRegs:$rs3)>;
}
multiclass ccMvPatsR<PatFrag CondOp, SodiumInst CondInst, SodiumInst A, SodiumInst B> {
def : Pat<(select (i16 (CondOp IntRegs:$rs1, IntRegs:$rs2)), IntRegs:$rs3, IntRegs:$T),
  (A IntRegs:$T, (CondInst IntRegs:$rs2, IntRegs:$rs1), IntRegs:$rs3)>;
def : Pat<(select (i16 (CondOp IntRegs:$rs1, IntRegs:$rs2)), IntRegs:$T, IntRegs:$rs3),
  (B IntRegs:$T, (CondInst IntRegs:$rs2, IntRegs:$rs1), IntRegs:$rs3)>;
}

defm : ccMvPats <seteq,  XOR,  MOVZ, MOVN>;
defm : ccMvPats <setne,  XOR,  MOVN, MOVZ>;

defm : ccMvPats <setge,  SLT,  MOVZ, MOVN>;
defm : ccMvPats <setuge, SLTU, MOVZ, MOVN>;
defm : ccMvPats <setlt,  SLT,  MOVN, MOVZ>;
defm : ccMvPats <setult, SLTU, MOVN, MOVZ>;

defm : ccMvPatsR<setle,  SLT,  MOVZ, MOVN>;
defm : ccMvPatsR<setule, SLTU, MOVZ, MOVN>;
defm : ccMvPatsR<setgt,  SLT,  MOVN, MOVZ>;
defm : ccMvPatsR<setugt, SLTU, MOVN, MOVZ>;

//Cond Branch
def : Pat<(brcond IntRegs:$rs, bb:$dst), (CBNZ IntRegs:$rs, simm20_brcc:$dst)>;

def : Pat<(brcond (i16 (seteq IntRegs:$rs,  0)), bb:$dst), (CBZ  IntRegs:$rs, simm20_brcc:$dst)>;
def : Pat<(brcond (i16 (setne IntRegs:$rs,  0)), bb:$dst), (CBNZ IntRegs:$rs, simm20_brcc:$dst)>;

def : Pat<(brcond (i16 (setgt IntRegs:$rs,  0)), bb:$dst), (CBGT IntRegs:$rs, simm20_brcc:$dst)>;
def : Pat<(brcond (i16 (setle IntRegs:$rs,  0)), bb:$dst), (CBLE IntRegs:$rs, simm20_brcc:$dst)>;
def : Pat<(brcond (i16 (setge IntRegs:$rs,  0)), bb:$dst), (CBGE IntRegs:$rs, simm20_brcc:$dst)>;
def : Pat<(brcond (i16 (setlt IntRegs:$rs,  0)), bb:$dst), (CBLT IntRegs:$rs, simm20_brcc:$dst)>;

def : Pat<(brcond (i16 (setgt IntRegs:$rs, -1)), bb:$dst), (CBGE IntRegs:$rs, simm20_brcc:$dst)>;
def : Pat<(brcond (i16 (setle IntRegs:$rs, -1)), bb:$dst), (CBLT IntRegs:$rs, simm20_brcc:$dst)>;
def : Pat<(brcond (i16 (setge IntRegs:$rs,  1)), bb:$dst), (CBGT IntRegs:$rs, simm20_brcc:$dst)>;
def : Pat<(brcond (i16 (setlt IntRegs:$rs,  1)), bb:$dst), (CBLE IntRegs:$rs, simm20_brcc:$dst)>;

class ccBrPats <PatFrag CondOp, SodiumInst CondInst, SodiumInst BrInst>
  : Pat<(brcond (i16 (CondOp IntRegs:$rs1, IntRegs:$rs2)), bb:$dst),
    (BrInst (CondInst IntRegs:$rs1, IntRegs:$rs2), simm20_brcc:$dst)>;
class ccBrPatsR<PatFrag CondOp, SodiumInst CondInst, SodiumInst BrInst>
  : Pat<(brcond (i16 (CondOp IntRegs:$rs1, IntRegs:$rs2)), bb:$dst),
    (BrInst (CondInst IntRegs:$rs2, IntRegs:$rs1), simm20_brcc:$dst)>;

def : ccBrPats <seteq,  XOR,  CBZ>;
def : ccBrPats <setne,  XOR,  CBNZ>;

def : ccBrPats <setge,  SLT,  CBZ>;
def : ccBrPats <setuge, SLTU, CBZ>;
def : ccBrPats <setlt,  SLT,  CBNZ>;
def : ccBrPats <setult, SLTU, CBNZ>;

def : ccBrPatsR<setle,  SLT,  CBZ>;
def : ccBrPatsR<setule, SLTU, CBZ>;
def : ccBrPatsR<setgt,  SLT,  CBNZ>;
def : ccBrPatsR<setugt, SLTU, CBNZ>;

//Cond Set
def : Pat<(seteq IntRegs:$rs, 0),  (SLTUI IntRegs:$rs, 1)>;
def : Pat<(setne IntRegs:$rs, 0),  (SLTU X0, IntRegs:$rs)>;
def : Pat<(setne IntRegs:$rs, -1), (SLTUI IntRegs:$rs, -1)>;

def : Pat<(seteq IntRegs:$rs1, IntRegs:$rs2), (SLTUI (XOR IntRegs:$rs1, IntRegs:$rs2), 1)>;
def : Pat<(setne IntRegs:$rs1, IntRegs:$rs2), (SLTU X0, (XOR IntRegs:$rs1, IntRegs:$rs2))>;
def : Pat<(setle IntRegs:$rs1, IntRegs:$rs2), (XORI (SLT  IntRegs:$rs2, IntRegs:$rs1), 1)>;
def : Pat<(setule IntRegs:$rs1, IntRegs:$rs2),(XORI (SLTU IntRegs:$rs2, IntRegs:$rs1), 1)>;
def : Pat<(setgt IntRegs:$rs1, IntRegs:$rs2), (SLT  IntRegs:$rs2, IntRegs:$rs1)>;
def : Pat<(setugt IntRegs:$rs1, IntRegs:$rs2),(SLTU IntRegs:$rs2, IntRegs:$rs1)>;
def : Pat<(setge IntRegs:$rs1, IntRegs:$rs2), (XORI (SLT IntRegs:$rs1, IntRegs:$rs2), 1)>;
def : Pat<(setuge IntRegs:$rs1, IntRegs:$rs2),(XORI (SLTU IntRegs:$rs1, IntRegs:$rs2), 1)>;

//Generic pattern classes
class PatReg<SDPatternOperator OpNode, SodiumInst Inst>
    : Pat<(i16 (OpNode (i16 IntRegs:$rs))), (Inst IntRegs:$rs)>;
class PatRegReg<SDPatternOperator OpNode, SodiumInst Inst>
    : Pat<(i16 (OpNode (i16 IntRegs:$rs1), (i16 IntRegs:$rs2))), (Inst IntRegs:$rs1, IntRegs:$rs2)>;
class PatRegImm<SDPatternOperator OpNode, SodiumInst Inst, ImmLeaf ImmType>
    : Pat<(i16 (OpNode (i16 IntRegs:$rs), ImmType:$imm)),
          (Inst IntRegs:$rs, ImmType:$imm)>;
class PatRegSimm13<SDPatternOperator OpNode, SodiumInst Inst>
    : PatRegImm<OpNode, Inst, simm13>;

//Arith
def : PatRegReg<add, ADD>;
def : PatRegSimm13<add, ADDI>;
def : PatRegReg<sub, SUB>;
def : PatRegReg<setlt, SLT>;
def : PatRegReg<setult, SLTU>;
def : PatRegSimm13<setlt, SLTI>;
def : PatRegSimm13<setult, SLTUI>;
def : PatRegReg<or, OR>;
def : PatRegSimm13<or, ORI>;
def : PatRegReg<and, AND>;
def : PatRegSimm13<and, ANDI>;
def : PatRegReg<xor, XOR>;
def : PatRegSimm13<xor, XORI>;
def : Pat<(not IntRegs:$rs), (XORI IntRegs:$rs, -1)>;
def : PatRegReg<shl, SLL>;
def : PatRegSimm13<shl, SLLI>;
def : PatRegReg<srl, SRL>;
def : PatRegSimm13<srl, SRLI>;
def : PatRegReg<sra, SRA>;
def : PatRegSimm13<sra, SRAI>;
def : PatRegReg<rotl, ROL>;
def : PatRegSimm13<rotl, ROLI>;

def ImmSubFrom16 : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(16 - N->getZExtValue(), SDLoc(N),
                                   N->getValueType(0));
}]>;
def : Pat<(i16 (rotr IntRegs:$rs1, IntRegs:$rs2)),  (ROL IntRegs:$rs1, (SUB X0, IntRegs:$rs2))>;
def : Pat<(i16 (rotr IntRegs:$rs1, simm13:$imm13)), (ROLI IntRegs:$rs1, (ImmSubFrom16 simm13:$imm13))>;

def : Pat<(i16 (add (shl (i16 IntRegs:$rs1), uimm3_plus1:$flag7), (i16 IntRegs:$rs2))),
  (SHADD IntRegs:$rs1, IntRegs:$rs2, uimm3_plus1:$flag7)>;

//Muplication
def : PatRegReg<mul, MUL>;
def : PatRegReg<mulhs, MULH>;
def : PatRegReg<mulhu, MULHU>;
def : PatRegSimm13<mul, MULI>;
def : PatRegSimm13<mul, MULHI>;

//Bit Manipulation
def : PatReg<ctlz, CLZ>;
def : PatReg<bitreverse, REV>;
def : PatRegReg<smin, MIN>;
def : PatRegReg<smax, MAX>;
def : PatRegReg<umin, MINU>;
def : PatRegReg<umax, MAXU>;
def : Pat<(i16 (cttz (i16 IntRegs:$rs))),  (CLZ (REV IntRegs:$rs))>;
def : Pat<(i16 (bswap (i16 IntRegs:$rs))), (ROLI IntRegs:$rs, 8)>;

def zexti8 : ComplexPattern<i16, 1, "SelectZExtBits<8>">;
def : Pat<(i16 (or (zexti8 (i16 IntRegs:$rs1)), (shl IntRegs:$rs2, (i16 8)))),
  (PACK IntRegs:$rs1, IntRegs:$rs2, 8)>;
def : Pat<(i16 (sext_inreg IntRegs:$rs1, i8)), (SBFX IntRegs:$rs1, 0, 7)>; //sext.8 $rs1 = sbfx $rs1, 0, 7

def : Pat<(i16 (and IntRegs:$rs1, (not IntRegs:$rs2))), (ANDN IntRegs:$rs1, IntRegs:$rs2)>;
def : Pat<(i16 (or  IntRegs:$rs1, (not IntRegs:$rs2))), (ORN  IntRegs:$rs1, IntRegs:$rs2)>;
def : Pat<(i16 (xor IntRegs:$rs1, (not IntRegs:$rs2))), (XNOR IntRegs:$rs1, IntRegs:$rs2)>;

//===----------------------------------------------------------------------===//
// Special codegen patterns
//===----------------------------------------------------------------------===//
// Addressing modes. ref: SodiumComplexPattern.h
def BaseAddr : ComplexPattern<iPTR, 1, "SelectBaseAddr">;
def AddrConstant : ComplexPattern<iPTR, 2, "SelectAddrConstant">;

def AddLike: PatFrags<(ops node:$A, node:$B),
                      [(add node:$A, node:$B), (or node:$A, node:$B)], [{
    return N->getOpcode() == ISD::ADD || isOrEquivalentToAdd(N);
}]>;

def : Pat<(AddLike (i16 BaseAddr:$rb), simm13:$imm13),
          (ADDI (i16 BaseAddr:$rb), simm13:$imm13)>;

multiclass LdPat<PatFrag LoadOp, SodiumInst Inst> {
  def : Pat<(i16 (LoadOp BaseAddr:$rb)), (Inst BaseAddr:$rb, 0)>;
  def : Pat<(i16 (LoadOp (AddrConstant IntRegs:$rb, simm13:$imm13))),
  (Inst IntRegs:$rb, simm13:$imm13)>;
  def : Pat<(i16 (LoadOp (AddLike BaseAddr:$rb, simm13:$imm13))),
  (Inst BaseAddr:$rb, simm13:$imm13)>;
}

multiclass StPat<PatFrag StoreOp, SodiumInst Inst> {
  def : Pat<(StoreOp (i16 IntRegs:$rd), BaseAddr:$rb),
  (Inst IntRegs:$rd, BaseAddr:$rb, 0)>;
  def : Pat<(StoreOp (i16 IntRegs:$rd), (AddrConstant IntRegs:$rb, simm13:$imm13)),
  (Inst IntRegs:$rd, IntRegs:$rb, simm13:$imm13)>;
  def : Pat<(StoreOp (i16 IntRegs:$rd), (AddLike BaseAddr:$rb, simm13:$imm13)),
  (Inst IntRegs:$rd, BaseAddr:$rb, simm13:$imm13)>;
}

defm : LdPat<load, LW>;
defm : LdPat<sextloadi8, LB>;
defm : LdPat<extloadi8, LBU>;
defm : LdPat<zextloadi8, LBU>;
defm : StPat<store, SW>;
defm : StPat<truncstorei8, SB>;

//TODO: dual word load/store
//def : LdPat<load, LD, i32>;
//def : StPat<store, SD, GPR, i32>;

def : Pat<(debugtrap), (EBREAK)>;
