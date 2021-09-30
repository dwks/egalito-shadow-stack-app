Welcome to the Egalito shadow stack exercise. Your goal is to build a
constant-offset shadow stack transformation tool. An app framework has been
provided, and you must add the following functionality:

1. Allocate 10MB shadow stack at address real stack - 0xb00000 (2*10MB).

2. Add the following at every function prologue:
    push   %r11
    mov    0x8(%rsp),%r11
    mov    %r11,-0xb00000(%rsp)
    pop    %r11

3. Add the following at every function epilogue:
    pushfd
    push   %r11
    mov    0x8(%rsp),%r11
    cmp    %r11,-0xb00000(%rsp)
    jne    shadowstack_violation
    pop    %r11
    popfd

4. Inject the following target function to jump to for violations:
    egalito_shadowstack_violation:
      hlt

The various places that need code added are marked with EXERCISE, please
grep for this. You can view the solution with "git diff solution".

The code can be built on its own with "make". You can test it with

$ make && cd test && make
$ ../app/etapp -q vuln vuln.ss
[...]
$ perl vuln.pl ./vuln.ss
spawn child process {./vuln.ss}
child wrote {buf is at 0x7fff6c22c2f0, target is at 0x55ed93e3e11e}
exploit is {AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA}
child process output {}
child process output {buf: [AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA]}
child process output {successful exploit! congratulations.}
child exited with status 1

This executes an exploit against the vulnerable program vuln. Your goal is to
make this attack fail. The exit status will show 11 (SIGSEGV) if you caused a
crash, and status 4 (SIGILL) if you successfully hit the shadow stack violation
function. For more details on how to run this test case, see test/README.txt.
