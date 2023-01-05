# illumos-ymm

This repo demonstrates an apparent illumos bug where parts of %ymm registers are
clobbered by taking a signal.

The program does this:

1. Sets up signal handlers.
2. Writes a known value (64-bit integers 1, 2, 3, and 4) to %ymm0.
3. Enters an infinite loop waiting for the value in %ymm0 to change.  Upon
   change, it prints out what it found.

The signal handler immediately reads %ymm0 and prints what it finds.  If the
signal is SIGINT or SIGUSR1, it writes a different known value (64-bit integers
5, 6, 7, and 8) to %ymm0, reads it back, and prints that out, too, for good
measure.

Start it up:

```
$ gmake
...
$ ./checkregs
main: test value (expect zeros): 0x0 0x0 0x0 0x0
main: after update, test value: 0x1 0x2 0x3 0x4
main: writing to %ymm0 and busy-waiting for it to change
```

Now hit it with SIGUSR2, which basically does nothing:

```
handle_sig: found loop_counter = 4694448974, %ymm0 = 0x1 0x2 0x3 0x4 (NO clobber)

handle_sig: found loop_counter = 7975071888, %ymm0 = 0x1 0x2 0x3 0x4 (NO clobber)
```

Now hit it with SIGINT, which clobbers %ymm0:

````
^C
handle_sig: found loop_counter = 11865090863, %ymm0 = 0x1 0x2 0x3 0x4 (CLOBBER)
    now %ymm0 = 0x5 0x6 0x7 0x8
main: after loop (loop_counter = 11865090864), test value: 0x1 0x2 0x7 0x8
```

The low 128 bits are the same, but the high 128 bits are different.

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
