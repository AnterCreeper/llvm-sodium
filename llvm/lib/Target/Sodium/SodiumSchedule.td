//===-- SodiumSchedule.td - Sodium Scheduling Definitions --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// ALU operations
def ReadIALU        : SchedRead;
def WriteIALU       : SchedWrite;

// Branch
def ReadBranch      : SchedRead;
def WriteBranch     : SchedWrite;

// CSR read & write
def ReadCSR         : SchedRead;
def WriteCSR        : SchedWrite;

// Mem load & store
def ReadMemBase     : SchedRead;
def ReadStoreData   : SchedRead;
def WriteLoad       : SchedWrite;
def WriteStore      : SchedWrite;

def SodiumGenericModel : SchedMachineModel {
    let MicroOpBufferSize = 0;
    let IssueWidth = 1;
    let LoadLatency = 1;
    let MispredictPenalty = 1;
    let CompleteModel = false;
}

let BufferSize = 0 in {
def SodiumUnitB   : ProcResource<1>; // Branch
def SodiumUnitALU : ProcResource<1>; // ALU
def SodiumUnitMem : ProcResource<1>; // Load/Store
def SodiumUnitCsr : ProcResource<1>; // CSR
}

let SchedModel = SodiumGenericModel in {

let Latency = 0 in {
def : WriteRes<WriteBranch, [SodiumUnitB]>;
}

let Latency = 1 in {
def : WriteRes<WriteIALU,   [SodiumUnitALU]>;
def : WriteRes<WriteLoad,   [SodiumUnitMem]>;
def : WriteRes<WriteStore,  [SodiumUnitMem]>;
}

let Latency = 3 in {
def : WriteRes<WriteCSR,    [SodiumUnitCsr]>;
}

def : ReadAdvance<ReadIALU, 0>;
def : ReadAdvance<ReadBranch, 0>;
def : ReadAdvance<ReadCSR, 0>;
def : ReadAdvance<ReadMemBase, 1>;
def : ReadAdvance<ReadStoreData, 0>;
}
