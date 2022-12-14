# illumos-ymm

This repo demonstrates an apparent illumos bug where parts of %ymm registers are
clobbered by taking a signal.

The program does this:

1. Sets up signal handlers.
2. Writes a known value (64-bit integers 1, 2, 3, and 4) to %ymm0.
3. Enters an infinite loop waiting for a signal.  *After* handling any signal,
   it prints out the value of %ymm0, sets it back to the known value, and waits
   for another signal so you can try again.

The signal handler immediately reads %ymm0 and prints what it finds.  If the
signal is SIGINT or SIGUSR1, it writes a different known value (64-bit integers
5, 6, 7, and 8) to %ymm0, reads it back, and prints that out, too, for good
measure.

Start it up:

```
$ gcc -march=native -m64 -Wall -o checkregs main.c asm.s  && ./checkregs 
main: test value (expect zeros): 0x0 0x0 0x0 0x0
main: %ymm0: 0x1 0x2 0x3 0x4
main: waiting for SIGINT, SIGUSR1, or SIGUSR2
```

Now hit it with SIGINT:

```
^C
handle_sig: found %ymm0 = 0xff0000000000 0x0 0x3 0x4 (CLOBBER)
    now %ymm0 = 0x5 0x6 0x7 0x8
main: saw signal
main: %ymm0: 0xff000000000000 0x0 0x3 0x4
main: resetting ymm0
main: %ymm0: 0x1 0x2 0x3 0x4
```

In that case, `main` found a different %ymm0 than it had before.  The low 128
bits are the same, but the high 128 bits are different.  Then it reset %ymm0 so
we can try it again:

```
^C
handle_sig: found %ymm0 = 0x1 0x2 0x3 0x4 (CLOBBER)
    now %ymm0 = 0x5 0x6 0x7 0x8
main: saw signal
main: %ymm0: 0xff000000000000 0x0 0x3 0x4
main: resetting ymm0
main: %ymm0: 0x1 0x2 0x3 0x4
```

Same story, though this time the signal handler at least found the value that
`main()` had left.

Now let's do it with SIGUSR2, whose handler doesn't write to %ymm0:

```
handle_sig: found %ymm0 = 0x1 0x2 0x3 0x4 (NO clobber)
main: saw signal
main: %ymm0: 0xff000000000000 0x0 0x3 0x4
main: resetting ymm0
main: %ymm0: 0x1 0x2 0x3 0x4
```

Same story.  Again:

```
handle_sig: found %ymm0 = 0x1 0x2 0x3 0x4 (NO clobber)
main: saw signal
main: %ymm0: 0xff000000000000 0x0 0x3 0x4
main: resetting ymm0
main: %ymm0: 0x1 0x2 0x3 0x4
```

Same story.

## Notes

I initially tried using the Intel intrinsics to read/write using the %ymm
registers.  The compiler outsmarted me.  When I read back the value after the
signal handler ran, it looked correct!  That's because the compiler happened to
restore the value into %ymm0 from the stack value before reading it back.

Some references for future-me:

* https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html
* https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95483
* https://stackoverflow.com/questions/57313195/inline-assembly-code-to-read-write-xmm-ymm-registers
* https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90129
