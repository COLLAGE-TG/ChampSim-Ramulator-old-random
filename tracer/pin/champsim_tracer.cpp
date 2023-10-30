/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 /*! @file
  *  This is an example of the PIN tool that demonstrates some basic PIN APIs
  *  and could serve as the starting point for developing your first PIN tool
  */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "../../inc/ChampSim/trace_instruction.h"

using trace_instr_format_t = input_instr;

// #define GC_START "signal_gc_start"
#define GC_START "GC_stopped_mark"

/* ================================================================== */
// Global variables 
/* ================================================================== */

UINT64 instrCount = 0;

std::ofstream outfile;

trace_instr_format_t curr_instr;

/* ================================================================== */
// Taiga's debug function
/* ================================================================== */

void file_output(std::string file_path)
{
  std::ifstream inputfile(file_path);
  if (!inputfile.is_open())
  {
    std::cerr << "ファイルを開けませんでした." << std::endl;
    exit(1);
  }
  // ファイルからデータを読み込んで標準出力に出力
  std::string line;
  while (std::getline(inputfile, line))
  {
    std::cout << line << std::endl;
  }
  // ファイルを閉じる
  inputfile.close();
}

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "champsim.trace",
                                 "specify file name for Champsim tracer output");

KNOB<UINT64> KnobSkipInstructions(KNOB_MODE_WRITEONCE, "pintool", "s", "0",
                                  "How many instructions to skip before tracing begins");

KNOB<UINT64> KnobTraceInstructions(KNOB_MODE_WRITEONCE, "pintool", "t", "1000000",
                                   "How many instructions to trace");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
  std::cerr << "This tool creates a register and memory access trace" << std::endl
    << "Specify the output trace file with -o" << std::endl
    << "Specify the number of instructions to skip before tracing with -s" << std::endl
    << "Specify the number of instructions to trace with -t" << std::endl << std::endl;

  std::cerr << KNOB_BASE::StringKnobSummary() << std::endl;

  return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

void ResetCurrentInstruction(VOID* ip)
{
  curr_instr = {};
  curr_instr.ip = (unsigned long long int)ip;
}

BOOL ShouldWrite()
{
  ++instrCount;
  return (instrCount > KnobSkipInstructions.Value()) && (instrCount <= (KnobTraceInstructions.Value() + KnobSkipInstructions.Value()));
}

void WriteCurrentInstruction()
{
  typename decltype(outfile)::char_type buf[sizeof(trace_instr_format_t)];
  std::memcpy(buf, &curr_instr, sizeof(trace_instr_format_t));
  outfile.write(buf, sizeof(trace_instr_format_t)); //curr_instrのデータをbufに渡す。curr_instrには.ip, .is_branchなどの値がある。
}

void BranchOrNot(UINT32 taken)
{
  curr_instr.is_branch = 1;
  curr_instr.branch_taken = taken;
}

template <typename T>
void WriteToSet(T* begin, T* end, UINT32 r)
{
  auto set_end = std::find(begin, end, 0);
  auto found_reg = std::find(begin, set_end, r); // check to see if this register is already in the list
  *found_reg = r; //すでに存在しているレジスタならそのまま、存在していなければ末尾に追加。
}

/* ===================================================================== */
// Print routine function
/* ===================================================================== */

// VOID do_call(ADDRINT addr)
// {
//   printf("%p - %s\n", (void*)addr, RTN_FindNameByAddress(addr).c_str());
//   //fprintf(trace, "\n[%s]\n",  RTN_FindNameByAddress(addr).c_str())); 
//   //fflush(trace);
// }

// // 関数の終了を知らせる関数
// VOID do_call_end(ADDRINT addr)
// {
//   std::cout << "end " << RTN_FindNameByAddress(addr).c_str() << std::endl;
// }

// // 関数エントリ時のコールバック関数
// VOID FunctionEntry(RTN rtn, VOID* v)
// {
//   // 関数名を取得
//   // std::string funcName = RTN_Name(rtn);
//   ADDRINT addr = RTN_Address(rtn);
//   std::string funcName = RTN_FindNameByAddress(addr).c_str();

//   // 関数エントリ時の処理をここに追加
//   std::cout << "Entering function: " << funcName << std::endl;

// }

// // 関数エグジット時のコールバック関数
// VOID FunctionExit(RTN rtn, VOID* v)
// {
//   // 関数名を取得
//   // std::string funcName = RTN_Name(rtn);
//   ADDRINT addr = RTN_Address(rtn);
//   std::string funcName = RTN_FindNameByAddress(addr).c_str();

//   // 関数エグジット時の処理をここに追加
//   std::cout << "Exiting function: " << funcName << std::endl;
// }

VOID Print_rtn_start(CHAR* name)
{
  std::cout << "====================" << "rtn_name =  " << name << "====================" << std::endl;
}

VOID Print_rtn_end(CHAR* name)
{
  std::cout << "====================" << " end " << name << "====================" << std::endl;
}

VOID Image(IMG img, VOID* v)
{
  RTN GC_start_rtn = RTN_FindByName(img, GC_START);
  if (RTN_Valid(GC_start_rtn))
  {
    std::cout << "====================" << "Finded GC_start_rtn " << GC_START << "====================" << std::endl;
    RTN_Open(GC_start_rtn);
    RTN_InsertCall(GC_start_rtn, IPOINT_BEFORE, (AFUNPTR)Print_rtn_start, IARG_ADDRINT, GC_START,
                   IARG_END);
    RTN_InsertCall(GC_start_rtn, IPOINT_AFTER, (AFUNPTR)Print_rtn_end, IARG_ADDRINT, GC_START, IARG_END);
    RTN_Close(GC_start_rtn);
  }
}


/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID* v)
{
  // begin each instruction with this function
  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ResetCurrentInstruction, IARG_INST_PTR, IARG_END);

  // instrument branch instructions
  if (INS_IsBranch(ins))
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)BranchOrNot, IARG_BRANCH_TAKEN, IARG_END);

  /* ===================================================================== */
  // Print routine
  /* ===================================================================== */
  // if (INS_IsCall(ins))
  // {
  //   if (INS_IsDirectControlFlow(ins))
  //   {
  //     const ADDRINT addr = INS_DirectControlFlowTargetAddress(ins);
  //     INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(do_call),
  //                              IARG_PTR, addr, IARG_FUNCARG_CALLSITE_VALUE, 0, IARG_END);
  //     RTN cur_rtn = RTN_FindByAddress(addr);
  //     // RTN cur_rtn = INS_Rtn(ins);

  //     // // 関数の最後に挿入される命令を指定
  //     RTN_Open(cur_rtn);
  //     INS tail_ins = RTN_InsTail(cur_rtn);

  //     // // 指定した命令にカスタムコードを挿入
  //     INS_InsertCall(tail_ins, IPOINT_BEFORE, AFUNPTR(do_call_end), IARG_PTR, addr, IARG_END);
  //     RTN_Close(cur_rtn);
  //   }
  // }
  /* ===================================================================== */
  // Print routine
  /* ===================================================================== */

  // instrument register reads
  UINT32 readRegCount = INS_MaxNumRRegs(ins);
  for (UINT32 i = 0; i < readRegCount; i++)
  {
    UINT32 regNum = INS_RegR(ins, i);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned char>,
                   IARG_PTR, curr_instr.source_registers, IARG_PTR, curr_instr.source_registers + NUM_INSTR_SOURCES,
                   IARG_UINT32, regNum, IARG_END);
  }

  // instrument register writes
  UINT32 writeRegCount = INS_MaxNumWRegs(ins);
  for (UINT32 i = 0; i < writeRegCount; i++)
  {
    UINT32 regNum = INS_RegW(ins, i);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned char>,
                   IARG_PTR, curr_instr.destination_registers, IARG_PTR, curr_instr.destination_registers + NUM_INSTR_DESTINATIONS,
                   IARG_UINT32, regNum, IARG_END);
  }

  // instrument memory reads and writes
  UINT32 memOperands = INS_MemoryOperandCount(ins);

  // Iterate over each memory operand of the instruction.
  for (UINT32 memOp = 0; memOp < memOperands; memOp++)
  {
    if (INS_MemoryOperandIsRead(ins, memOp))
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned long long int>,
                     IARG_PTR, curr_instr.source_memory, IARG_PTR, curr_instr.source_memory + NUM_INSTR_SOURCES,
                     IARG_MEMORYOP_EA, memOp, IARG_END);
    if (INS_MemoryOperandIsWritten(ins, memOp))
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned long long int>,
                     IARG_PTR, curr_instr.destination_memory, IARG_PTR, curr_instr.destination_memory + NUM_INSTR_DESTINATIONS,
                     IARG_MEMORYOP_EA, memOp, IARG_END);
  }

  // finalize each instruction with this function
  INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)ShouldWrite, IARG_END);
  INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteCurrentInstruction, IARG_END);
}

/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID* v)
{
  outfile.close();
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments,
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char* argv[])
{

  // === for print routine name ===
  // Initialize symbol table code, needed for rtn instrumentation
  PIN_InitSymbols();
  // === for print routine name ===

  // Initialize PIN library. Print help message if -h(elp) is specified
  // in the command line or the command line is invalid 
  if (PIN_Init(argc, argv))
    return Usage();

  outfile.open(KnobOutputFile.Value().c_str(), std::ios_base::binary | std::ios_base::trunc);
  if (!outfile)
  {
    std::cout << "Couldn't open output trace file. Exiting." << std::endl;
    exit(1);
  }

  // // RTN_AddInstrumentFunctionを使用して関数エントリとエグジットのコールバック関数を設定
  // RTN_AddInstrumentFunction(FunctionEntry, 0);
  // RTN_AddInstrumentFunction(FunctionExit, 0);


  // プリントファンクション
  IMG_AddInstrumentFunction(Image, 0);

  // Register function to be called to instrument instructions
  INS_AddInstrumentFunction(Instruction, 0);

  // Register function to be called when the application exits
  PIN_AddFiniFunction(Fini, 0);

  // Start the program, never returns
  // このプログラムからは戻ってこない。アプリケーションの実行を開始する。
  PIN_StartProgram();

  return 0;
}