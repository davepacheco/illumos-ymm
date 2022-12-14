/*
 * test program to examine the behavior of preserving ymm registers across
 * signal handlers
 * References:
 * https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95483
 * https://stackoverflow.com/questions/57313195/inline-assembly-code-to-read-write-xmm-ymm-registers
 *
 * Other potentially relevant bugs I may have run into:
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90129
 *
 * TODO then I tried:
 *   value = _mm256_maskz_load_epi64(~0, &initial_value[0]);
 *   _mm256_store_epi64(&test_value, value);
 * and got SIGILL on:
 *   kmovw  %ecx,%k1
 */

#include <immintrin.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>

static int gotsig;
void read_ymm0(uint64_t val[4]);
void write_ymm0(uint64_t val[4]);
volatile __m256i sig_value;

void
handle_sig(int sig, siginfo_t *sip, void *arg)
{
	uint64_t clobber_value[4];

	(void) write(STDOUT_FILENO, "in signal handler\n", sizeof ("in signal handler\n"));

	clobber_value[0] = (uint64_t)'b';
	clobber_value[1] = (uint64_t)'o';
	clobber_value[2] = (uint64_t)'o';
	clobber_value[3] = (uint64_t)'m';

	// write_ymm0(&clobber_value[0]);
	sig_value = _mm256_lddqu_si256((__m256i *)&clobber_value);

	gotsig = 1;
}

int
main(int argc, char *argv[])
{
	uint64_t initial_value[4];
	uint64_t test_value[4];
	struct sigaction sa_sig;
	__m256i regvalue;

	// Register signal handlers.
	// SIGINT is more convenient for interactive use.
	// SIGUSR1 is more convenient when run under mdb.
	bzero(&sa_sig, sizeof (sa_sig));
	sa_sig.sa_sigaction = handle_sig;
	sa_sig.sa_flags = SA_SIGINFO | SA_RESETHAND;
	(void) sigaction(SIGINT, &sa_sig, NULL);
	(void) sigaction(SIGUSR1, &sa_sig, NULL);

	// Write a known initial value to %ymm0.
	initial_value[0] = 1;
	initial_value[1] = 2;
	initial_value[2] = 3;
	initial_value[3] = 4;
	regvalue = _mm256_lddqu_si256((__m256i *)&initial_value);
	_mm256_store_si256((__m256i *)&test_value, regvalue);
	// write_ymm0(&initial_value[0]);

	// Read it back and print it out.  bzero the test_value first so that we
	// know we've actually successfully read the right thing.
	bzero(test_value, sizeof (test_value));
	(void) printf("before: zero'd test_value\n");
	(void) printf("value: 0x%lx 0x%lx 0x%lx 0x%lx\n",
	    test_value[0], test_value[1], test_value[2], test_value[3]);

	(void) printf("before: test_value from %%ymm0\n");
	// read_ymm0(&test_value[0]);
	_mm256_store_si256((__m256i *)&test_value, regvalue);
	(void) printf("value: 0x%lx 0x%lx 0x%lx 0x%lx\n",
	    test_value[0], test_value[1], test_value[2],
	    test_value[3]);

	// Wait patiently for a signal to arrive.
	printf("main: waiting for SIGINT or SIGUSR1\n");
	while (gotsig == 0) {
		sleep(1);
	}

	(void) printf("main: saw signal\n");
	_mm256_store_si256((__m256i *)&test_value, regvalue);
	// read_ymm0(&test_value[0]);
	(void) printf("%%ymm: 0x%lx 0x%lx 0x%lx 0x%lx\n",
	    test_value[0], test_value[1], test_value[2],
	    test_value[3]);
	return (0);
}
