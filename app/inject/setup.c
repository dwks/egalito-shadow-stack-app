#define _GNU_SOURCE
#include <sys/mman.h>
#include <unistd.h>

#define SHADOW_STACK_SIZE (10*1024*1024)

void egalito_allocate_shadow_stack(void) {
    // EXERCISE: use mmap to allocate a shadow stack of size SHADOW_STACK_SIZE,
    // precisely SHADOW_STACK_SIZE bytes below the main stack. Thus, code can
    // use %rsp-0xb00000 and hit accessible memory.
    //
    // This function is injected by compiling it to a separate shared library.
}
