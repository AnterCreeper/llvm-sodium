//===-- SodiumSchedule.td - Sodium Scheduling Definitions --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

/// Define scheduler resources associated with def operands.
def WriteIALU       : SchedWrite;    // ALU operations
def WriteJmp        : SchedWrite;    // Uncond Jump
def WriteJal        : SchedWrite;    // Uncond Jump and link
def WriteJalr       : SchedWrite;    // Uncond Jump and link register
def WriteBranch     : SchedWrite;    // Cond Branch
def WriteLoad       : SchedWrite;    // Load
def WriteStore      : SchedWrite;    // Store
def WriteCSR        : SchedWrite;    // CSR read & write

/// Define scheduler resources associated with use operands.
def ReadIALU        : SchedRead;
def ReadJmp         : SchedRead;
def ReadJalr        : SchedRead;
def ReadCSR         : SchedRead;
def ReadMemBase     : SchedRead;
def ReadStoreData   : SchedRead;

def SodiumGenericModel : SchedMachineModel {
    let MicroOpBufferSize = 0; // in-order.
    let IssueWidth = 1;        // 1 micro-op is dispatched per cycle.
    let LoadLatency = 1;
    let MispredictPenalty = 3;
    let CompleteModel = false;
}

let BufferSize = 0 in {
    def SodiumUnitALU   : ProcResource<1>; // ALU
    def SodiumUnitMem   : ProcResource<1>; // Load/Store
    def SodiumUnitCsr   : ProcResource<1>; // CSR
    def SodiumUnitB     : ProcResource<1>; // Branch
}

/*
let SchedModel = SodiumGenericModel in {

let Latency = 0 in {
def : WriteRes<WriteJmp,    [SodiumUnitB]>;
def : WriteRes<WriteJal,    [SodiumUnitB]>;
def : WriteRes<WriteJalr,   [SodiumUnitB]>;
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

def ReadIALU        : SchedRead;
def ReadJmp         : SchedRead;
def ReadJalr        : SchedRead;
def ReadCSR         : SchedRead;
def ReadMemBase     : SchedRead;
def ReadStoreData   : SchedRead;

// Bypass and advance
def : ReadAdvance<ReadIALU, 0>;
def : ReadAdvance<ReadJmp,  0>;
def : ReadAdvance<ReadJalr, 0>;
def : ReadAdvance<ReadCSR,  0>;
def : ReadAdvance<ReadMemBase, 0>;
def : ReadAdvance<ReadStoreData, 0>;
}
*/
