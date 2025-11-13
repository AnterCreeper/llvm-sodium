ParseStatus SodiumAsmParser::parseMemOpBaseReg(OperandVector &Operands) {
  if (parseToken(AsmToken::LParen, "expected '('"))
    return ParseStatus::Failure;
  Operands.push_back(SodiumOperand::createToken("(", getLoc()));

  if (!parseRegister(Operands).isSuccess())
    return Error(getLoc(), "expected register");

  if (parseToken(AsmToken::RParen, "expected ')'"))
    return ParseStatus::Failure;
  Operands.push_back(SodiumOperand::createToken(")", getLoc()));

  return ParseStatus::Success;
}

ParseStatus SodiumAsmParser::parseOperandWithModifier(OperandVector &Operands) {
  SMLoc S = getLoc();
  SMLoc E;

  if (parseToken(AsmToken::Percent, "expected '%' for operand modifier"))
    return ParseStatus::Failure;

  if (getLexer().getKind() != AsmToken::Identifier)
    return Error(getLoc(), "expected valid identifier for operand modifier");
  StringRef Identifier = getParser().getTok().getIdentifier();
  SodiumMCExpr::VariantKind VK = SodiumMCExpr::getVariantKindForName(Identifier);
  if (VK == SodiumMCExpr::VK_SODIUM_Invalid)
    return Error(getLoc(), "unrecognized operand modifier");

  getParser().Lex(); // Eat the identifier
  if (parseToken(AsmToken::LParen, "expected '('"))
    return ParseStatus::Failure;

  const MCExpr *SubExpr;
  if (getParser().parseParenExpression(SubExpr, E))
    return ParseStatus::Failure;

  const MCExpr *ModExpr = SodiumMCExpr::create(SubExpr, VK, getContext());
  Operands.push_back(SodiumOperand::createImm(ModExpr, S, E));
  return ParseStatus::Success;
}

ParseStatus SodiumAsmParser::parseImmediate(OperandVector &Operands) {
  SMLoc S = getLoc();
  SMLoc E;
  const MCExpr *Res;

  switch (getLexer().getKind()) {
  default:
    return ParseStatus::NoMatch;
  case AsmToken::LParen:
  case AsmToken::Dot:
  case AsmToken::Minus:
  case AsmToken::Plus:
  case AsmToken::Exclaim:
  case AsmToken::Tilde:
  case AsmToken::Integer:
  case AsmToken::String:
  case AsmToken::Identifier:
    if (getParser().parseExpression(Res, E))
      return ParseStatus::Failure;
    break;
  case AsmToken::Percent:
    return parseOperandWithModifier(Operands);
  }

  Operands.push_back(SodiumOperand::createImm(Res, S, E));
  return ParseStatus::Success;
}

ParseStatus SodiumAsmParser::parseCallSymbol(OperandVector &Operands) {
  SMLoc S = getLoc();
  const MCExpr *Res;

  if (getLexer().getKind() != AsmToken::Identifier)
    return ParseStatus::NoMatch;

  StringRef Identifier;
  if (getParser().parseIdentifier(Identifier))
    return ParseStatus::Failure;

  SMLoc E = SMLoc::getFromPointer(S.getPointer() + Identifier.size());

  MCSymbol *Sym = getContext().getOrCreateSymbol(Identifier);
  Res = MCSymbolRefExpr::create(Sym, MCSymbolRefExpr::VK_None, getContext());
  Res = SodiumMCExpr::create(Res, SodiumMCExpr::VK_SODIUM_CALL, getContext());
  Operands.push_back(SodiumOperand::createImm(Res, S, E));
  return ParseStatus::Success;
}

ParseStatus SodiumAsmParser::parseBareSymbol(OperandVector &Operands) {
  SMLoc S = getLoc();
  const MCExpr *Res;

  if (getLexer().getKind() != AsmToken::Identifier)
    return ParseStatus::NoMatch;

  StringRef Identifier;
  AsmToken Tok = getLexer().getTok();
  if (getParser().parseIdentifier(Identifier))
    return ParseStatus::Failure;

  SMLoc E = SMLoc::getFromPointer(S.getPointer() + Identifier.size());

  MCSymbol *Sym = getContext().getOrCreateSymbol(Identifier);

  if (Sym->isVariable()) {
    const MCExpr *V = Sym->getVariableValue(/*SetUsed=*/false);
    if (!isa<MCSymbolRefExpr>(V)) {
      getLexer().UnLex(Tok); // Put back if it's not a bare symbol.
      return ParseStatus::NoMatch;
    }
    Res = V;
  } else
    Res = MCSymbolRefExpr::create(Sym, MCSymbolRefExpr::VK_None, getContext());

  MCBinaryExpr::Opcode Opcode;
  switch (getLexer().getKind()) {
  default:
    Operands.push_back(SodiumOperand::createImm(Res, S, E));
    return ParseStatus::Success;
  case AsmToken::Plus:
    Opcode = MCBinaryExpr::Add;
    getLexer().Lex();
    break;
  case AsmToken::Minus:
    Opcode = MCBinaryExpr::Sub;
    getLexer().Lex();
    break;
  }

  const MCExpr *Expr;
  if (getParser().parseExpression(Expr, E))
    return ParseStatus::Failure;
  Res = MCBinaryExpr::create(Opcode, Res, Expr, getContext());
  Operands.push_back(SodiumOperand::createImm(Res, S, E));
  return ParseStatus::Success;
}

ParseStatus SodiumAsmParser::parseCSRSystemRegister(OperandVector &Operands) {
  SMLoc S = getLoc();
  const MCExpr *Res;

  switch (getLexer().getKind()) {
    default:
      return ParseStatus::NoMatch;
    case AsmToken::LParen:
    case AsmToken::Minus:
    case AsmToken::Plus:
    case AsmToken::Exclaim:
    case AsmToken::Tilde:
    case AsmToken::Integer:
    case AsmToken::String: {
      if (getParser().parseExpression(Res))
        return ParseStatus::Failure;

      auto *CE = dyn_cast<MCConstantExpr>(Res);
      if (CE) {
        int64_t Imm = CE->getValue();
        if (isUInt<13>(Imm)) {
          auto SysReg = SodiumSysReg::lookupSysRegByEncoding(Imm);
          // Accept an immediate representing a named or un-named Sys Reg
          // if the range is valid, regardless of the required features.
          Operands.push_back(
            SodiumOperand::createSysReg(SysReg ? SysReg->Name : "", S, Imm));
          return ParseStatus::Success;
        }
      }
      return generateImmOutOfRangeError(S, 0, (1 << 13) - 1);
    }
    case AsmToken::Identifier: {
      StringRef Identifier;
      if (getParser().parseIdentifier(Identifier))
        return ParseStatus::Failure;

      auto SysReg = SodiumSysReg::lookupSysRegByName(Identifier);
      if (SysReg) {
        Operands.push_back(
          SodiumOperand::createSysReg(Identifier, S, SysReg->Encoding));
        return ParseStatus::Success;
      }
      return generateImmOutOfRangeError(S, 0, (1 << 13) - 1,
                                        "operand must be a valid system register "
                                        "name or an integer in the range");
    }
    case AsmToken::Percent: {
      // Discard operand with modifier.
      return generateImmOutOfRangeError(S, 0, (1 << 13) - 1);
    }
  }

  return ParseStatus::NoMatch;
}
