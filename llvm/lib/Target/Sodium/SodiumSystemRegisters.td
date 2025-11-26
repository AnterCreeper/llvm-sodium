//===- SodiumSystemRegisters.td ----------------------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the system registers of Sodium Architecture.
//
//===----------------------------------------------------------------------===//

include "llvm/TableGen/SearchableTable.td"

//===----------------------------------------------------------------------===//
// CSR (control and status register read/write) instruction options.
//===----------------------------------------------------------------------===//

class SysReg<string name, bits<13> op> {
  string Name = name;
  bits<13> Encoding = op;
}

def SysRegsList : GenericTable {
  let FilterClass = "SysReg";
  let Fields = [
    "Name", "Encoding"
  ];
  let PrimaryKey = [ "Encoding" ];
  let PrimaryKeyName = "lookupSysRegByEncoding";
}

def lookupSysRegByName : SearchIndex {
  let Table = SysRegsList;
  let Key = [ "Name" ];
}

def : SysReg<"mvec", 0x0000>;
def : SysReg<"mepc", 0x0001>;
def : SysReg<"msta", 0x0002>;
def : SysReg<"mask", 0x0003>;

def : SysReg<"mcr0", 0x0010>;
def : SysReg<"mcr1", 0x0011>;
def : SysReg<"mcr2", 0x0012>;
def : SysReg<"mcr3", 0x0013>;

def : SysReg<"mstk", 0x0014>;
