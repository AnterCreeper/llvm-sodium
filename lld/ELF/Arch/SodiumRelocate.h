#define CASE_PCREL_ADD(len) {       \
  int64_t diff = (int64_t)val;      \
  diff  += BIT(len);                \
  diff >>= len + 1;                 \
  checkInt(loc, diff, 20, rel);     \
  return (insn & GENMASK(0, 11)) |  \
    (GETBITS(diff, 15, 19) << 12) | \
    (GETBITS(diff, 0,  14) << 17);  \
}

static uint64_t getReloc(uint8_t *loc, const Relocation &rel, uint64_t val) {
  uint64_t insn = read32le(loc);
  switch (rel.type) {
  default:
    llvm_unreachable("unknown relocation");
  case R_SODIUM_PCREL_ADD:
    CASE_PCREL_ADD(0)
  case R_SODIUM_PCREL_ADD12:
    CASE_PCREL_ADD(12)
  case R_SODIUM_PCREL_ADD20:
    CASE_PCREL_ADD(20)
  case R_SODIUM_LO16: {
    val = GETBITS(val, 0, 15);
    return (insn & GENMASK(0, 11))  |
      (GETBITS(val, 15, 19) << 12)  |
      (GETBITS(val, 0,  14) << 17);
  }
  case R_SODIUM_HI16: {
    val = GETBITS(val, 16, 31);
    return (insn & GENMASK(0, 11))  |
      (GETBITS(val, 15, 19) << 12)  |
      (GETBITS(val, 0,  14) << 17);
  }
  case R_SODIUM_PCREL_LO13I: {
    return (insn & GENMASK(0, 16)) | //16:0
      (GETBITS(val, 1, 12) << 17)  | //28:17
      (GETBITS(val, 0, 0)  << 29)  | //29:29
      (insn & GENMASK(31, 30));      //31:30
  }
  case R_SODIUM_PCREL_LO13L: {
    return (insn & GENMASK(0, 3))  | //3:0
      (GETBITS(val, 1, 5)   << 4)  | //8:4
      (insn & GENMASK(9, 21))      | //21:9
      (GETBITS(val, 6, 12)  << 22) | //28:22
      (GETBITS(val, 0, 0)   << 29) | //29:29
      (insn & GENMASK(31, 30));      //31:30
  }
  case R_SODIUM_BR20: {
    checkInt(loc, val, 21, rel);
    checkAlignment(loc, val, 2, rel);
    return (insn & GENMASK(0, 3))  | //3:0
      (GETBITS(val, 1,  5)  << 4)  | //8:4
      (insn & GENMASK(9, 11))      | //11:9
      (GETBITS(val, 16, 20) << 12) | //16:12
      (insn & GENMASK(17, 21))     | //21:17
      (GETBITS(val, 6,  15) << 22);  //31:22
  }
  case R_SODIUM_BR25: {
    checkInt(loc, val, 26, rel);
    checkAlignment(loc, val, 2, rel);
    return (insn & GENMASK(0, 3))  | //3:0
      (GETBITS(val, 1,  5)  << 4)  | //8:4
      (insn & GENMASK(9, 11))      | //11:9
      (GETBITS(val, 16, 25) << 12) | //21:12
      (GETBITS(val, 6,  15) << 22);  //31:22
  }
  }
  return 0; //unreachable
}
