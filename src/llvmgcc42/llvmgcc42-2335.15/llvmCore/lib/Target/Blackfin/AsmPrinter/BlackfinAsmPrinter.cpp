//===-- BlackfinAsmPrinter.cpp - Blackfin LLVM assembly writer ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to GAS-format BLACKFIN assembly language.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "asm-printer"
#include "Blackfin.h"
#include "BlackfinInstrInfo.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Module.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/DwarfWriter.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Target/Mangler.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetRegistry.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
using namespace llvm;

namespace {
  class BlackfinAsmPrinter : public AsmPrinter {
  public:
    BlackfinAsmPrinter(formatted_raw_ostream &O, TargetMachine &TM,
                       MCStreamer &Streamer)
      : AsmPrinter(O, TM, Streamer) {}

    virtual const char *getPassName() const {
      return "Blackfin Assembly Printer";
    }

    void printOperand(const MachineInstr *MI, int opNum);
    void printMemoryOperand(const MachineInstr *MI, int opNum);
    void printInstruction(const MachineInstr *MI);  // autogenerated.
    static const char *getRegisterName(unsigned RegNo);

    void EmitInstruction(const MachineInstr *MI) {
      printInstruction(MI);
      OutStreamer.AddBlankLine();
    }
    bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                         unsigned AsmVariant, const char *ExtraCode);
    bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                               unsigned AsmVariant, const char *ExtraCode);
  };
} // end of anonymous namespace

#include "BlackfinGenAsmWriter.inc"

extern "C" void LLVMInitializeBlackfinAsmPrinter() {
  RegisterAsmPrinter<BlackfinAsmPrinter> X(TheBlackfinTarget);
}

void BlackfinAsmPrinter::printOperand(const MachineInstr *MI, int opNum) {
  const MachineOperand &MO = MI->getOperand (opNum);
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    assert(TargetRegisterInfo::isPhysicalRegister(MO.getReg()) &&
           "Virtual registers should be already mapped!");
    O << getRegisterName(MO.getReg());
    break;

  case MachineOperand::MO_Immediate:
    O << MO.getImm();
    break;
  case MachineOperand::MO_MachineBasicBlock:
    O << *MO.getMBB()->getSymbol();
    return;
  case MachineOperand::MO_GlobalAddress:
    O << *Mang->getSymbol(MO.getGlobal());
    printOffset(MO.getOffset());
    break;
  case MachineOperand::MO_ExternalSymbol:
    O << *GetExternalSymbolSymbol(MO.getSymbolName());
    break;
  case MachineOperand::MO_ConstantPoolIndex:
    O << MAI->getPrivateGlobalPrefix() << "CPI" << getFunctionNumber() << "_"
      << MO.getIndex();
    break;
  case MachineOperand::MO_JumpTableIndex:
    O << MAI->getPrivateGlobalPrefix() << "JTI" << getFunctionNumber()
      << '_' << MO.getIndex();
    break;
  default:
    llvm_unreachable("<unknown operand type>");
    break;
  }
}

void BlackfinAsmPrinter::printMemoryOperand(const MachineInstr *MI, int opNum) {
  printOperand(MI, opNum);

  if (MI->getOperand(opNum+1).isImm() && MI->getOperand(opNum+1).getImm() == 0)
    return;

  O << " + ";
  printOperand(MI, opNum+1);
}

/// PrintAsmOperand - Print out an operand for an inline asm expression.
///
bool BlackfinAsmPrinter::PrintAsmOperand(const MachineInstr *MI,
                                         unsigned OpNo,
                                         unsigned AsmVariant,
                                         const char *ExtraCode) {
  if (ExtraCode && ExtraCode[0]) {
    if (ExtraCode[1] != 0) return true; // Unknown modifier.

    switch (ExtraCode[0]) {
    default: return true;  // Unknown modifier.
    case 'r':
      break;
    }
  }

  printOperand(MI, OpNo);

  return false;
}

bool BlackfinAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                               unsigned OpNo,
                                               unsigned AsmVariant,
                                               const char *ExtraCode) {
  if (ExtraCode && ExtraCode[0])
    return true;  // Unknown modifier

  O << '[';
  printOperand(MI, OpNo);
  O << ']';

  return false;
}
