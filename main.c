/*
 * test program to examine the behavior of preserving ymm registers across
 * signal handlers
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>

// set by the signal handler to wake up main()
static volatile uint64_t nsigs;
// updated by "main"
static volatile uint64_t loop_counter;

// signal handler
static void handle_sig(int, siginfo_t *, void *);

// functions defined in asm.s
void read_ymm0(uint64_t val[4]);
void write_ymm0(uint64_t val[4]);

int
main(int argc, char *argv[])
{
	uint64_t initial_value[4] = { 1, 2, 3, 4 };
	uint64_t test_value[4];
	struct sigaction sa_sig;

	// Register signal handler that will clobber %ymm0.
	// SIGINT is more convenient for interactive use.
	// SIGUSR1 is more convenient when run under mdb.
	bzero(&sa_sig, sizeof (sa_sig));
	sa_sig.sa_sigaction = handle_sig;
	sa_sig.sa_flags = SA_SIGINFO;
	(void) sigaction(SIGINT, &sa_sig, NULL);
	(void) sigaction(SIGUSR1, &sa_sig, NULL);
	(void) sigaction(SIGUSR2, &sa_sig, NULL);

	// Write a known value into %ymm0, read it back, and verify that it
	// matches what we expect.  This really just tests that our own
	// functions do what we think they do.
	bzero(test_value, sizeof (test_value));
	(void) printf(
	    "main: test value (expect zeros): 0x%lx 0x%lx 0x%lx 0x%lx\n",
	    test_value[0], test_value[1], test_value[2], test_value[3]);
	write_ymm0(&initial_value[0]);
	read_ymm0(&test_value[0]);
	(void) printf(
	    "main: after update, test value: 0x%lx 0x%lx 0x%lx 0x%lx\n",
	    test_value[0], test_value[1], test_value[2], test_value[3]);

	// Write a known initial value to %ymm0 and wait for it to change.
	// %ymm0 is caller-saved, so there must be no calls to any functions
	// here except ones that we know don't modify these.
	(void) printf(
	    "main: writing to %%ymm0 and busy-waiting for it to change\n");
	write_ymm0(&initial_value[0]);
	// Wait for it to change.
	for (loop_counter = 0; ; loop_counter++) {
		read_ymm0(&test_value[0]);

		// We avoid bcmp since it's a function call that could use %ymm0.
		if (test_value[0] != initial_value[0] ||
		    test_value[1] != initial_value[1] ||
		    test_value[2] != initial_value[2] ||
		    test_value[3] != initial_value[3]) {
			break;
		}
	}

	(void) printf(
	    "main: after loop (loop_counter = %lu), "
	    "test value: 0x%lx 0x%lx 0x%lx 0x%lx\n",
	    loop_counter, test_value[0], test_value[1], test_value[2],
	    test_value[3]);

	return (0);
}

// Handles SIGINT, SIGUSR1, or SIGUSR2.
// On SIGINT or SIGUSR1, clobbers %ymm0.  Otherwise, does basically nothing.
static void
handle_sig(int sig, siginfo_t *sip, void *arg)
{
	char buf[256];
	int nbytes;
	int doclobber;
	uint64_t found_value[4];

	// Read %ymm0 and print what we found.
	read_ymm0(&found_value[0]);
	doclobber = sig == SIGINT || sig == SIGUSR1;
	nbytes = snprintf(buf, sizeof (buf),
	    "\nhandle_sig: found loop_counter = %lu, "
	    "%%ymm0 = 0x%lx 0x%lx 0x%lx 0x%lx (%s)\n",
	    loop_counter, found_value[0], found_value[1], found_value[2],
	    found_value[3], doclobber ? "CLOBBER" : "NO clobber");
	(void) write(STDOUT_FILENO, buf, nbytes);

	// If this was SIGINT or SIGUSR1, clobber %ymm0.  Print it out again so
	// we know we got it right.
	if (doclobber) {
		uint64_t clobber_value[4] = { 5, 6, 7, 8 };
		write_ymm0(&clobber_value[0]);
		read_ymm0(&found_value[0]);
		nbytes = snprintf(buf, sizeof (buf),
		    "    now %%ymm0 = 0x%lx 0x%lx 0x%lx 0x%lx\n",
		    found_value[0], found_value[1], found_value[2],
		    found_value[3]);
		(void) write(STDOUT_FILENO, buf, nbytes);
	}

	// Perform a syscall (go off-CPU).  TODO ask Robert why.
	(void) sleep(1);

	nsigs++;
}
