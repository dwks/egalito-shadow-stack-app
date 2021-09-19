#ifndef EGALITO_PASS_SHADOW_STACK_H
#define EGALITO_PASS_SHADOW_STACK_H

#include "pass/chunkpass.h"

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
    void pushToShadowStack(Function *function);
    void popFromShadowStack(Instruction *instruction);
};


#endif
