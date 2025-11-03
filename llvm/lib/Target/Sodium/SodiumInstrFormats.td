//===-- SodiumInstrFormats.td - Sodium Instruction Formats -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

class SodiumInst<dag outs, dag ins, string opcodestr, string argstr, list<dag> pattern>: Instruction
{
  bits<4> opcode;
  field bits<32> Inst;

  let Namespace = "Sodium";
  let DecoderNamespace = "Sodium";

  let Pattern = pattern;
  dag InOperandList = ins;
  dag OutOperandList = outs;
  let AsmString = opcodestr # !if(!empty(argstr), "", "\t" # argstr);

  // SoftFail is a field the disassembler can use to provide a way for
  // instructions to not match without killing the whole decode process. It is
  // mainly used for ARM, but Tablegen expects this field to exist or it fails
  // to build the decode table.
  field bits<32> SoftFail = 0;

  let Size = 4;
  let Inst{3-0} = opcode;

  bit IsSignExt = 0;
  let TSFlags{3-0} = opcode;
}

//===----------------------------------------------------------------------===//
// Pseudo
//===----------------------------------------------------------------------===//
class Pseudo<dag outs, dag ins, string opcodestr = "", string argstr = "", list<dag> pattern>
  : SodiumInst<outs, ins, opcodestr, argstr, pattern> {
  let opcode = 0b0000;
  let isCodeGenOnly = 1;
}  

//===----------------------------------------------------------------------===//
// FMT_J(Jump) and FMT_B(Branch)
//===----------------------------------------------------------------------===//
class FMT_J<bits<3> func3, bits<5> rb, dag outs, dag ins, string opcodestr, string argstr, list<dag> pattern>
  : SodiumInst<outs, ins, opcodestr, argstr, pattern>
{
  let opcode      = 0b1001;
  let Inst{31-22} = 0;
  let Inst{21-17} = rb;
  let Inst{16-12} = 0;
  let Inst{11-9}  = func3;
  let Inst{8-4}   = 0;
}

class FMT_Ji<bits<3> func3, dag outs, dag ins, string opcodestr, string argstr, list<dag> pattern>
  : SodiumInst<outs, ins, opcodestr, argstr, pattern>
{
  bits<25> imm25;

  let opcode      = 0b1001;
  let Inst{31-22} = imm25{14-5};
  let Inst{21-12} = imm25{24-15};
  let Inst{11-9}  = func3;
  let Inst{8-4}   = imm25{4-0};
}

class FMT_Jr<bits<3> func3, dag outs, dag ins, string opcodestr, string argstr, list<dag> pattern>
  : SodiumInst<outs, ins, opcodestr, argstr, pattern>
{
  bits<5> rb;
  bits<20> imm20;

  let opcode      = 0b1001;
  let Inst{31-22} = imm20{14-5};
  let Inst{21-17} = rb;
  let Inst{16-12} = imm20{19-15};
  let Inst{11-9}  = func3;
  let Inst{8-4}   = imm20{4-0};
}

class FMT_B<bits<3> func3, dag outs, dag ins, string opcodestr, string argstr>
  : SodiumInst<outs, ins, opcodestr, argstr, []>
{
  bits<20> imm20;
  bits<5> rs;

  let opcode      = 0b0001;
  let Inst{31-22} = imm20{14-5};
  let Inst{21-17} = rs;
  let Inst{16-12} = imm20{19-15};
  let Inst{11-9}  = func3;
  let Inst{8-4}   = imm20{4-0};
}

//===----------------------------------------------------------------------===//
// FMT_R(Register) and FMT_I(Immediate)
//===----------------------------------------------------------------------===//
class FMT_R<bits<3> func3, bits<3> tag3, bits<7> flag7, dag outs, dag ins, string opcodestr, string argstr>
  : SodiumInst<outs, ins, opcodestr, argstr, []>
{
  bits<5> rd;
  bits<5> rs1;
  bits<5> rs2;

  let opcode      = 0b0111;
  let Inst{31-29} = tag3;
  let Inst{28-22} = flag7;
  let Inst{21-17} = rs2;
  let Inst{16-12} = rs1;
  let Inst{11-9}  = func3;
  let Inst{8-4}   = rd;
}

//variation of FMT_R for shadd
class FMT_S<bits<3> func3, bits<7> flag7, dag outs, dag ins, string opcodestr, string argstr>
  : SodiumInst<outs, ins, opcodestr, argstr, []>
{
  bits<3> shamt3;
  bits<5> rd;
  bits<5> rs1;
  bits<5> rs2;

  let opcode      = 0b0111;
  let Inst{31-29} = shamt3;
  let Inst{28-22} = flag7;
  let Inst{21-17} = rs2;
  let Inst{16-12} = rs1;
  let Inst{11-9}  = func3;
  let Inst{8-4}   = rd;
}

class FMT_I<bits<3> func3, bits<3> tag3, dag outs, dag ins, string opcodestr, string argstr>
  : SodiumInst<outs, ins, opcodestr, argstr, []>
{
  bits<13> imm13;
  bits<5> rd;
  bits<5> rs;

  let opcode      = 0b1111;
  let Inst{31-30} = tag3{2-1};
  let Inst{29}    = imm13{0};
  let Inst{28-17} = imm13{12-1};
  let Inst{16-12} = rs;
  let Inst{11-9}  = func3;
  let Inst{8-4}   = rd;
}

//variation of FMT_I with 7bit flag as immediate
class FMT_RI<bits<3> func3, bits<3> tag3, dag outs, dag ins, string opcodestr, string argstr>
  : SodiumInst<outs, ins, opcodestr, argstr, []>
{
  bits<5> rd;
  bits<5> rs1;
  bits<5> rs2;
  bits<7> flag7;

  let opcode      = 0b0111;
  let Inst{31-29} = tag3;
  let Inst{28-22} = flag7;
  let Inst{21-17} = rs2;
  let Inst{16-12} = rs1;
  let Inst{11-9}  = func3;
  let Inst{8-4}   = rd;
}

//===----------------------------------------------------------------------===//
// FMT_LS(Load Store) and FMT_SR(Special Register)
//===----------------------------------------------------------------------===//
class FMT_LS<bits<3> func3, bits<2> tag2, dag outs, dag ins, string opcodestr, string argstr>
  : SodiumInst<outs, ins, opcodestr, argstr, []>
{
  bits<13> imm13;
  bits<5> rb;
  bits<5> rs;

  let opcode      = 0b0101;
  let Inst{31-30} = tag2;
  let Inst{29}    = imm13{0};
  let Inst{28-22} = imm13{12-6};
  let Inst{21-17} = rb;
  let Inst{16-12} = rs;
  let Inst{11-9}  = func3;
  let Inst{8-4}   = imm13{5-1};
}

//Same as FMT_LS, but use different opcode.
class FMT_SR<bits<3> func3, bits<2> tag2, dag outs, dag ins, string opcodestr, string argstr>
  : SodiumInst<outs, ins, opcodestr, argstr, []>
{
  bits<13> imm13;
  bits<5> rb;
  bits<5> rs;

  let opcode      = 0b1101;
  let Inst{31-30} = tag2;
  let Inst{29}    = imm13{0};
  let Inst{28-22} = imm13{12-6};
  let Inst{21-17} = rb;
  let Inst{16-12} = rs;
  let Inst{11-9}  = func3;
  let Inst{8-4}   = imm13{5-1};
}

//===----------------------------------------------------------------------===//
// FMT_LRA(Load Relative Address)
//===----------------------------------------------------------------------===//
class FMT_LRA<bits<3> func3, dag outs, dag ins, string opcodestr, string argstr>
  : SodiumInst<outs, ins, opcodestr, argstr, []>
{
  bits<20> imm20;
  bits<5> rd;

  let opcode      = 0b1011;
  let Inst{31-17} = imm20{14-0};
  let Inst{16-12} = imm20{19-15};
  let Inst{11-9}  = func3;
  let Inst{8-4}   = rd;
}

//===----------------------------------------------------------------------===//
// FMT_HT(Hint)
//===----------------------------------------------------------------------===//
class FMT_HT<bits<3> func3, bits<3> tag3, bits<7> flag7, dag outs, dag ins, string opcodestr, string argstr>
  : SodiumInst<outs, ins, opcodestr, argstr, []>
{
  bits<20> imm20;
  bits<5> rd;

  let opcode      = 0b0011;
  let Inst{31-29} = tag3;
  let Inst{28-22} = flag7;
  let Inst{21-12} = 0;
  let Inst{11-9}  = func3;
  let Inst{8-4}   = 0;
}
