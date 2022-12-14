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
static volatile int gotsig;

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

	// Write a known initial value to %ymm0.
	write_ymm0(&initial_value[0]);

	// Read it back and print it out.  bzero the test_value first so that we
	// know we've actually successfully read the right thing.
	bzero(test_value, sizeof (test_value));
	(void) printf(
	    "main: test value (expect zeros): 0x%lx 0x%lx 0x%lx 0x%lx\n",
	    test_value[0], test_value[1], test_value[2], test_value[3]);

	read_ymm0(&test_value[0]);
	(void) printf("main: %%ymm0: 0x%lx 0x%lx 0x%lx 0x%lx\n",
	    test_value[0], test_value[1], test_value[2], test_value[3]);

	// Wait patiently for a signal to arrive.
	printf("main: waiting for SIGINT, SIGUSR1, or SIGUSR2\n");
	for (;;) {
		while (gotsig == 0) {
			uint8_t c;
			int r = read(0, &c, 1);
			if (r == 0) {
				// Stop on end-of-input.
				goto done;
			}
			if (r == -1 && errno != EINTR) {
				// Bail on any unexpected error.
				perror("read");
				goto done;
			}
		}

		// Print what we found in %ymm0.
		(void) printf("main: saw signal\n");
		read_ymm0(&test_value[0]);
		(void) printf("main: %%ymm0: 0x%lx 0x%lx 0x%lx 0x%lx\n",
		    test_value[0], test_value[1], test_value[2],
		    test_value[3]);

		// Reset %ymm0 so that we can try again.
		(void) printf("main: resetting ymm0\n");
		write_ymm0(&initial_value[0]);
		read_ymm0(&test_value[0]);
		(void) printf("main: %%ymm0: 0x%lx 0x%lx 0x%lx 0x%lx\n",
		    test_value[0], test_value[1], test_value[2],
		    test_value[3]);
		gotsig = 0;
	}

done:
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
	    "\nhandle_sig: found %%ymm0 = 0x%lx 0x%lx 0x%lx 0x%lx (%s)\n",
	    found_value[0], found_value[1], found_value[2], found_value[3],
	    doclobber ? "CLOBBER" : "NO clobber");
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

	// Tell main() to proceed.
	gotsig = sig;
}
