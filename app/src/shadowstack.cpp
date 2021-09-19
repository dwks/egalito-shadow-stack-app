#include <vector>
#include <cassert>
#include "shadowstack.h"
#include "disasm/disassemble.h"
#include "instr/register.h"
#include "instr/concrete.h"
#include "operation/mutator.h"
#include "operation/addinline.h"
#include "operation/find2.h"
#include "pass/switchcontext.h"
#include "types.h"

void ShadowStackPass::visit(Program *program) {
    if(auto f = dynamic_cast<Function *>(program->getEntryPoint())) {
        entryPoint = f;
    }

    addStackAllocationHook(program);
    recurse(program);
}

void ShadowStackPass::visit(Module *module) {
    this->violationTarget = makeViolationTarget(module);
    recurse(module);
}

void ShadowStackPass::addStackAllocationHook(Program *program) {
    // add call to shadow stack allocation function in __libc_start_main
    auto allocateFunc = ChunkFind2(program).findFunction(
        "egalito_allocate_shadow_stack");
    assert(allocateFunc && "ShadowStackPass requires libinject.so to be parsed");

    auto sourceFunc = ChunkFind2(program).findFunction(
        "__libc_start_main");
    assert(sourceFunc && "ShadowStackPass requires libc to be present (uniongen)");

    SwitchContextPass switchContext;
    allocateFunc->accept(&switchContext);

    auto hook = makeStackAllocationCall(allocateFunc);

    auto block1 = sourceFunc->getChildren()->getIterable()->get(0);
    {
        ChunkMutator m(block1, true);
        m.prepend(hook);
    }
}

Instruction *ShadowStackPass::makeStackAllocationCall(Function *allocateFunc) {
    auto call = new Instruction();
    auto callSem = new ControlFlowInstruction(
        X86_INS_CALL, call, "\xe8", "call", 4);
    callSem->setLink(new NormalLink(allocateFunc, Link::SCOPE_EXTERNAL_JUMP));
    call->setSemantic(callSem);
    return call;
}

Function *ShadowStackPass::makeViolationTarget(Module *module) {
    auto instr = Disassemble::instruction({0x0f, 0x0b});  // ud2
    auto block = new Block();

    auto symbol = new Symbol(0x0, 0, "egalito_shadowstack_violation",
       Symbol::TYPE_FUNC, Symbol::BIND_GLOBAL, 0, 0);
    auto function = new Function(symbol);
    function->setName(symbol->getName());
    function->setPosition(new AbsolutePosition(0x0));

    module->getFunctionList()->getChildren()->add(function);
    function->setParent(module->getFunctionList());
    ChunkMutator(function).append(block);
    ChunkMutator(block).append(instr);
    return function;
}

bool ShadowStackPass::isFunctionBlacklisted(Function *function) {
    auto name = function->getName();

    // Shadow stack pass uses these internally
    if(name == "egalito_shadowstack_violation") return true;
    if(name == "egalito_allocate_shadow_stack") return true;
    if(name == "mmap64") return true;
    if(name == "mmap") return true;

    // Needed for initialization
    if(name == "_start" || function == entryPoint) return true;
    if(name == "__libc_start_main") return true;

    // Needed to make long jumps work
    if(name == "__longjmp") return true;
    if(name == "__longjmp_chk") return true;

    // mempcpy does jmp into middle of __memcpy_avx_unaligned_erms
    if(name.find("memcpy") != std::string::npos) return true;

    // memcpy does jmp into middle of __memmove_sse2_unaligned_erms
    if(name.find("memmove") != std::string::npos) return true;

    // __memset_avx2_unaligned_erms has ja, conditional tail recursion
    if(name.find("memset") != std::string::npos) return true;

    // __strncasecmp_l_avx has jne, conditional tail recursion
    if(name.find("str") != std::string::npos) return true;

    // sphinx3, function does tail recursion to itself
    if(name == "mdef_phone_id") return true;

    // jne conditional tail recursion
    if(name == "obstack_free") return true;

    return false;
}

void ShadowStackPass::visit(Function *function) {
    if(isFunctionBlacklisted(function)) return;

    pushToShadowStack(function);
    recurse(function);
}

void ShadowStackPass::visit(Instruction *instruction) {
    auto semantic = instruction->getSemantic();
    if(/*auto v = */dynamic_cast<ReturnInstruction *>(semantic)) {
        popFromShadowStack(instruction);
    }
    else if(auto v = dynamic_cast<ControlFlowInstruction *>(semantic)) {
        if(v->getMnemonic() != "callq"
            && v->getLink() && v->getLink()->isExternalJump()) {  // tail recursion

            popFromShadowStack(instruction);
        }
    }
    else if(auto v = dynamic_cast<IndirectJumpInstruction *>(semantic)) {
        if(!v->isForJumpTable()) {  // indirect tail recursion
            popFromShadowStack(instruction);
        }
    }
    else if(auto v = dynamic_cast<DataLinkedControlFlowInstruction *>(semantic)) {
        if(!v->isCall()) {
            popFromShadowStack(instruction);
        }
    }
    /*else if(dynamic_cast<IndirectCallInstruction *>(semantic)) {
        popFromShadowStack(instruction);
    }*/
}

#define FITS_IN_ONE_BYTE(x) ((x) < 0x80)  /* signed 1-byte operand */
#define GET_BYTE(x, shift) static_cast<unsigned char>(((x) >> (shift*8)) & 0xff)
#define GET_BYTES(x) GET_BYTE((x),0), GET_BYTE((x),1), GET_BYTE((x),2), GET_BYTE((x),3)

void ShadowStackPass::pushToShadowStack(Function *function) {
    ChunkAddInline ai({X86_REG_R11}, [] (unsigned int stackBytesAdded) {
        // 0:   41 53                   push   %r11
        // 2:   4c 8b 5c 24 08          mov    0x8(%rsp),%r11
        // 7:   4c 89 9c 24 00 00 50    mov    %r11,-0xb00000(%rsp)
        // e:   ff
        // f:   41 5b                   pop    %r11

        Instruction *mov1Instr = nullptr;
        if(FITS_IN_ONE_BYTE(stackBytesAdded)) {
            mov1Instr = Disassemble::instruction({0x4c, 0x8b, 0x5c, 0x24, GET_BYTE(stackBytesAdded,0)});
        } else {
            //   5:    4c 8b 9c 24 88 00 00    mov    0x88(%rsp),%r11
            //   c:    00 
            mov1Instr = Disassemble::instruction({0x4c, 0x8b, 0x9c, 0x24, GET_BYTES(stackBytesAdded)});
        }
        Instruction *mov2Instr = nullptr;
        {
            unsigned int offset = -0xb00000 + stackBytesAdded;
            mov2Instr = Disassemble::instruction({0x4c, 0x89, 0x9c, 0x24, GET_BYTES(offset)});
        }

        return std::vector<Instruction *>{ mov1Instr, mov2Instr };
    });
	auto block1 = function->getChildren()->getIterable()->get(0);
	auto instr1 = block1->getChildren()->getIterable()->get(0);
    ai.insertBefore(instr1, false);
}

void ShadowStackPass::popFromShadowStack(Instruction *instruction) {
    ChunkAddInline ai({X86_REG_EFLAGS, X86_REG_R11}, [this] (unsigned int stackBytesAdded) {
        /*
                                         pushfd
            0:   41 53                   push   %r11
            2:   4c 8b 5c 24 08          mov    0x8(%rsp),%r11
            7:   4c 39 9c 24 00 00 50    cmp    %r11,-0xb00000(%rsp)
            e:   ff
            f:   0f 85 00 00 00 00       jne    0x15
           15:   41 5b                   pop    %r11
                                         popfd
        */
        Instruction *movInstr = nullptr;
        if(FITS_IN_ONE_BYTE(stackBytesAdded)) {
            movInstr = Disassemble::instruction({0x4c, 0x8b, 0x5c, 0x24, GET_BYTE(stackBytesAdded,0)});
        } else {
            //   5:    4c 8b 9c 24 88 00 00    mov    0x90(%rsp),%r11
            //   c:    00 
            // (optional 0x80 for redzone), 0x8 for pushfd, 0x8 for push %r11
            movInstr = Disassemble::instruction({0x4c, 0x8b, 0x9c, 0x24, GET_BYTES(stackBytesAdded)});
        }
        Instruction *cmpInstr = nullptr;
        {
            unsigned int offset = -0xb00000 + stackBytesAdded;
            cmpInstr = Disassemble::instruction({0x4c, 0x39, 0x9c, 0x24, GET_BYTES(offset)});
        }

        auto jne = new Instruction();
        auto jneSem = new ControlFlowInstruction(
            X86_INS_JNE, jne, "\x0f\x85", "jnz", 4);
        jneSem->setLink(new NormalLink(violationTarget, Link::SCOPE_EXTERNAL_JUMP));
        jne->setSemantic(jneSem);
        return std::vector<Instruction *>{ movInstr, cmpInstr, jne };
    });
    ai.insertBefore(instruction, true);
}

#undef FITS_IN_ONE_BYTE
#undef GET_BYTE
#undef GET_BYTES
