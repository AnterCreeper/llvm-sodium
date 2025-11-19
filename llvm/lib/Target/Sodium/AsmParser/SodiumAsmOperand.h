/// SodiumOperand - Instances of this class represent a parsed machine
/// instruction
struct SodiumOperand final : public MCParsedAsmOperand {
  enum class KindTy {
    Register,
    Immediate,
    Token,
    SystemRegister,
  } Kind;
  struct RegOp {
    MCRegister RegNum;
  };
  struct ImmOp {
    const MCExpr *Val;
  };
  struct SysRegOp {
    const char *Data;
    unsigned Length;
    unsigned Encoding;
  };

  SMLoc StartLoc, EndLoc;
  union {
    RegOp Reg;
    ImmOp Imm;
    StringRef Tok;
    struct SysRegOp SysReg;
  };
  SodiumOperand(KindTy K) : Kind(K) {}

public:
  SodiumOperand(const SodiumOperand &o) : MCParsedAsmOperand() {
    Kind = o.Kind;
    StartLoc = o.StartLoc;
    EndLoc = o.EndLoc;
    switch (Kind) {
    case KindTy::Register:
      Reg = o.Reg;
      break;
    case KindTy::Immediate:
      Imm = o.Imm;
      break;
    case KindTy::Token:
      Tok = o.Tok;
      break;
    case KindTy::SystemRegister:
      SysReg = o.SysReg;
      break;
    }
  }

  unsigned getReg() const override {
      assert(Kind == KindTy::Register && "Invalid type access!");
      return Reg.RegNum.id();
  }
  const MCExpr *getImm() const {
      assert(Kind == KindTy::Immediate && "Invalid type access!");
      return Imm.Val;
  }
  StringRef getToken() const {
      assert(Kind == KindTy::Token && "Invalid type access!");
      return Tok;
  }
  StringRef getSysReg() const {
      assert(Kind == KindTy::SystemRegister && "Invalid type access!");
      return StringRef(SysReg.Data, SysReg.Length);
  }

  bool isMem() const override { return false; }
  bool isImm() const override { return Kind == KindTy::Immediate; }
  bool isReg() const override { return Kind == KindTy::Register; }
  bool isToken() const override { return Kind == KindTy::Token; }
  bool isCSRSystemRegister() const { return Kind == KindTy::SystemRegister; }

  /// getStartLoc - Gets location of the first token of this operand
  SMLoc getStartLoc() const override { return StartLoc; }
  /// getEndLoc - Gets location of the last token of this operand
  SMLoc getEndLoc() const override { return EndLoc; }

  static std::unique_ptr<SodiumOperand> createImm(const MCExpr *Val, SMLoc S,
                                                  SMLoc E) {
    auto Op = std::make_unique<SodiumOperand>(KindTy::Immediate);
    Op->Imm.Val = Val;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }
  static std::unique_ptr<SodiumOperand> createReg(unsigned RegNo, SMLoc S,
                                                  SMLoc E) {
    auto Op = std::make_unique<SodiumOperand>(KindTy::Register);
    Op->Reg.RegNum = RegNo;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }
  static std::unique_ptr<SodiumOperand> createToken(StringRef Str, SMLoc S) {
    auto Op = std::make_unique<SodiumOperand>(KindTy::Token);
    Op->Tok = Str;
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }
  static std::unique_ptr<SodiumOperand> createSysReg(StringRef Str, SMLoc S,
                                                     unsigned Encoding) {
    auto Op = std::make_unique<SodiumOperand>(KindTy::SystemRegister);
    Op->SysReg.Data = Str.data();
    Op->SysReg.Length = Str.size();
    Op->SysReg.Encoding = Encoding;
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }

  void print(raw_ostream &OS) const override {
    auto RegName = [](MCRegister Reg) {
      if (Reg)
        return SodiumInstPrinter::getRegisterName(Reg);
      else
        return "noreg";
    };

    switch (Kind) {
    case KindTy::Immediate:
      OS << *getImm();
      break;
    case KindTy::Register:
      OS << "<register " << RegName(getReg()) << ">";
      break;
    case KindTy::Token:
      OS << "'" << getToken() << "'";
      break;
    case KindTy::SystemRegister:
      OS << "<sysreg: " << getSysReg() << '>';
      break;
    }
  }

  static bool evaluateConstantImm(const MCExpr *Expr, int64_t &Imm,
                                  SodiumMCExpr::VariantKind &VK) {
    if (auto *RE = dyn_cast<SodiumMCExpr>(Expr)) {
      VK = RE->getKind();
      return RE->evaluateAsConstant(Imm);
    }
    if (auto CE = dyn_cast<MCConstantExpr>(Expr)) {
      VK = SodiumMCExpr::VK_SODIUM_None;
      Imm = CE->getValue();
      return true;
    }
    return false;
  }

  static void addExpr(MCInst &Inst, const MCExpr *Expr) {
    assert(Expr && "Expr shouldn't be null!");
    int64_t Imm = 0;
    SodiumMCExpr::VariantKind VK = SodiumMCExpr::VK_SODIUM_None;
    bool IsConstant = evaluateConstantImm(Expr, Imm, VK);
    if (IsConstant)
      Inst.addOperand(MCOperand::createImm(SignExtend64<32>(Imm)));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }

#define FUNC(X, Y, Z) { \
  int64_t Imm; \
  SodiumMCExpr::VariantKind VK = SodiumMCExpr::VK_SODIUM_None; \
  if (!isImm()) \
    return false; \
  bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK); \
  bool IsValid; \
  if (!IsConstantImm) \
    IsValid = X; \
  else \
    IsValid = Y; \
  return IsValid && (Z); \
}

  template <unsigned N>
  bool IsUImmN() const
    FUNC(false,
         isUInt<N>(Imm),
         VK == SodiumMCExpr::VK_SODIUM_None)

  template <int N>
  bool isBareSimmNLsb0() const
    FUNC(SodiumAsmParser::classifySymbolRef(getImm(), VK),
         (isShiftedInt<N - 1, 1>(Imm)),
         VK == SodiumMCExpr::VK_SODIUM_None)

  // Predicate methods for AsmOperands defined in SodiumInstrInfo.td
  bool isBareSymbol() const
    FUNC(SodiumAsmParser::classifySymbolRef(getImm(), VK),
         false,
         VK == SodiumMCExpr::VK_SODIUM_None)

  bool isCallSymbol() const
    FUNC(SodiumAsmParser::classifySymbolRef(getImm(), VK),
         false,
         VK == SodiumMCExpr::VK_SODIUM_CALL)

  bool isSImm26Lsb0() const { return isBareSimmNLsb0<26>(); }
  bool isSImm21Lsb0() const { return isBareSimmNLsb0<21>(); }

  bool isUImm4() const { return IsUImmN<4>(); }

  bool isSImm16Lsb0() const
    FUNC(SodiumAsmParser::classifySymbolRef(getImm(), VK),
         (isShiftedInt<15, 1>(Imm)),
         (IsConstantImm && (VK == SodiumMCExpr::VK_SODIUM_None)) ||
         (VK == SodiumMCExpr::VK_SODIUM_PCREL_LO))
  bool isSImm13() const
    FUNC(SodiumAsmParser::classifySymbolRef(getImm(), VK),
         isInt<13>(Imm),
         (IsConstantImm && (VK == SodiumMCExpr::VK_SODIUM_None)) ||
         (VK == SodiumMCExpr::VK_SODIUM_PCREL_LO))

  bool isUImm3Sub1() const
    FUNC(false,
         isUInt<3>(Imm-1),
         VK == SodiumMCExpr::VK_SODIUM_None)

  bool isUImm16LI() const
    FUNC(SodiumAsmParser::classifySymbolRef(getImm(), VK),
         isUInt<16>(Imm),
         (IsConstantImm && (VK == SodiumMCExpr::VK_SODIUM_None)) ||
         (VK == SodiumMCExpr::VK_SODIUM_LO16) ||
         (VK == SodiumMCExpr::VK_SODIUM_HI16))

  bool isUImm20LA() const
    FUNC(SodiumAsmParser::classifySymbolRef(getImm(), VK),
         isUInt<20>(Imm),
         (IsConstantImm && (VK == SodiumMCExpr::VK_SODIUM_None)) ||
         (VK == SodiumMCExpr::VK_SODIUM_PCREL_ADD) ||
         (VK == SodiumMCExpr::VK_SODIUM_PCREL_ADD12) ||
         (VK == SodiumMCExpr::VK_SODIUM_PCREL_ADD20))

  // Used by the TableGen Code
  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::createReg(getReg()));
  }
  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }
  void addCSRSystemRegisterOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::createImm(SysReg.Encoding));
  }

};
} // end anonymous namespace.
