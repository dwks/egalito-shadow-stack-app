#ifndef EGALITO_PASS_SHADOW_STACK_H
#define EGALITO_PASS_SHADOW_STACK_H

#include "pass/chunkpass.h"

#ifndef ARCH_X86_64
    #error "ShadowStackPass does not support this architecture"
#endif

class ShadowStackPass : public ChunkPass {
private:
    Function *violationTarget;
    Function *entryPoint;
public:
    ShadowStackPass() : violationTarget(nullptr), entryPoint(nullptr) {}

    virtual void visit(Program *program);
    virtual void visit(Module *module);
    virtual void visit(Function *function);
    virtual void visit(Instruction *instruction);
private:
    void addStackAllocationHook(Program *program);
    Instruction *makeStackAllocationCall(Function *allocateFunc);
    Function *makeViolationTarget(Module *module);

    bool isFunctionBlacklisted(Function *function);
    void pushToShadowStack(Function *function);
    void popFromShadowStack(Instruction *instruction);
};


#endif
