/*
 * Copyright (C) 2004-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

 #include "pin.H"
 #include <iostream>
 #include <fstream>
 
 using std::cerr;
 using std::endl;
 using std::hex;
 using std::ios;
 using std::string;
 
 
 /* ===================================================================== */
 /* Global Variables */
 /* ===================================================================== */
 
 std::ofstream TraceFile;
 
 /* ===================================================================== */
 /* Commandline Switches */
 /* ===================================================================== */
 
 KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "funcTrace.out", "specify trace file name");
 
 /* ===================================================================== */
 
 /* ===================================================================== */
 /* Analysis routines                                                     */
 /* ===================================================================== */

// Uncomment MatmulEntry and MatmulExit if debug
//  BOOL isInMatmul = FALSE;
//  VOID MatmulEntry() {
//     isInMatmul = TRUE;
//     TraceFile << "=== ENTERING matmul function ===" << endl;
// }

// VOID MatmulExit() {
//     isInMatmul = FALSE;
//     TraceFile << "=== EXITING matmul function ===" << endl;
// }

 // Print a memory read record
 VOID RecordMemRead(ADDRINT ip, ADDRINT addr) {
    TraceFile << "r " << addr << endl;
 }

 // Print a memory write record
 VOID RecordMemWrite(ADDRINT ip, ADDRINT addr) {
    TraceFile << "w " << addr << endl;
 }
 
 /* ===================================================================== */
 /* Instrumentation routines                                              */
 /* ===================================================================== */
 
VOID FuncIns(IMG img, VOID* v)
{
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            string name = RTN_Name(rtn);
            if (name.find("matmul") != string::npos)
            {
                RTN_Open(rtn);
                for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
                {
                    UINT32 memOperands = INS_MemoryOperandCount(ins);
                    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
                    {
                        if (INS_MemoryOperandSize(ins, memOp) == sizeof(double))
                        {
                            if (INS_MemoryOperandIsRead(ins, memOp))
                            {
                                INS_InsertPredicatedCall(
                                    ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
                                    IARG_INST_PTR,
                                    IARG_MEMORYOP_EA, memOp,
                                    IARG_END);
                            }
                            if (INS_MemoryOperandIsWritten(ins, memOp))
                            {
                                INS_InsertPredicatedCall(
                                    ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
                                    IARG_INST_PTR,
                                    IARG_MEMORYOP_EA, memOp,
                                    IARG_END);
                            }
                        }
                    }
                }
                RTN_Close(rtn);
            }
        }
    }
}



 
 /* ===================================================================== */
 
 VOID Fini(INT32 code, VOID* v) { TraceFile.close(); }
 
 /* ===================================================================== */
 /* Print Help Message                                                    */
 /* ===================================================================== */
 
 INT32 Usage()
 {
     cerr << "This tool produces a memory access trace of calls to specific function." << endl;
     cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
     return -1;
 }
 
 /* ===================================================================== */
 /* Main                                                                  */
 /* ===================================================================== */
 
 int main(int argc, char* argv[])
 {
     // Initialize pin & symbol manager
     PIN_InitSymbols();
     if (PIN_Init(argc, argv))
     {
         return Usage();
     }
 
     // Write to a file since cout and cerr maybe closed by the application
     TraceFile.open(KnobOutputFile.Value().c_str());
     TraceFile << hex;
     TraceFile.setf(ios::showbase);
 
     // Register FuncIns to be called to instrument functions.
     IMG_AddInstrumentFunction(FuncIns, 0);
     PIN_AddFiniFunction(Fini, 0);
 
     // Never returns
     PIN_StartProgram();
 
     return 0;
 }