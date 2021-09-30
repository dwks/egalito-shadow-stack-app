#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>
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
    auto symbol = new Symbol(0x0, 0, "egalito_shadowstack_violation",
       Symbol::TYPE_FUNC, Symbol::BIND_GLOBAL, 0, 0);
    auto function = new Function(symbol);
    function->setName(symbol->getName());
    function->setPosition(new AbsolutePosition(0x0));

    module->getFunctionList()->getChildren()->add(function);
    function->setParent(module->getFunctionList());

    // EXERCISE: We are creating a new function from scratch and injecting it.
    // Please add a basic block with a single "ud2" instruction to the function.
    // ChunkMutator will be helpful.

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

void ShadowStackPass::pushToShadowStack(Function *function) {
    ChunkAddInline ai({X86_REG_R11}, [] (unsigned int stackBytesAdded) {
        // EXERCISE: Add the following instructions to each function prologue:
        //     0:   41 53                   push   %r11
        //     2:   4c 8b 5c 24 08          mov    0x8(%rsp),%r11
        //     7:   4c 89 9c 24 00 00 50    mov    %r11,-0xb00000(%rsp)
        //     e:   ff
        //     f:   41 5b                   pop    %r11
        // Note that ChunkAddInline automatically saves the given registers to the
        // stack, so the push/pop is automatically generated. You can use
        // Reassemble::instructions to make instructions out of strings.

        std::stringstream ss;
        // ...

        return Reassemble::instructions(ss.str());
    });
	auto block1 = function->getChildren()->getIterable()->get(0);
	auto instr1 = block1->getChildren()->getIterable()->get(0);
    ai.insertBefore(instr1, false);
}

void ShadowStackPass::popFromShadowStack(Instruction *instruction) {
    ChunkAddInline ai({X86_REG_EFLAGS, X86_REG_R11}, [this] (unsigned int stackBytesAdded) {
        // EXERCISE: Add the following instructions to each function epilogue:
        //                                   pushfd
        //      0:   41 53                   push   %r11
        //      2:   4c 8b 5c 24 08          mov    0x8(%rsp),%r11
        //      7:   4c 39 9c 24 00 00 50    cmp    %r11,-0xb00000(%rsp)
        //      e:   ff
        //      f:   0f 85 00 00 00 00       jne    0x15
        //     15:   41 5b                   pop    %r11
        //                                   popfd
        // Note that ChunkAddInline automatically saves the given registers to the
        // stack, so the push/pop is automatically generated. You can use
        // Reassemble::instructions to make instructions out of strings.

        std::stringstream ss;
        // ...

        auto jne = new Instruction();
        auto jneSem = new ControlFlowInstruction(
            X86_INS_JNE, jne, "\x0f\x85", "jnz", 4);
        jneSem->setLink(new NormalLink(violationTarget, Link::SCOPE_EXTERNAL_JUMP));
        jne->setSemantic(jneSem);

        auto instructions = Reassemble::instructions(ss.str());
        //instructions.push_back(jne);
        return instructions;
    });
    ai.insertBefore(instruction, true);
}
