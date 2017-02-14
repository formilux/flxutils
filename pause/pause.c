/*
 * Pause - program pausing forever - 2017-02-14 - Willy Tarreau <w@1wt.eu>
 * Useful to start a namespace and prevent it from exiting. Called with an
 * argument, it forks into background. It's suggested to call it with "-D"
 * in this case. Also supports being built with nolibc :
 *
 * $CC -fno-asynchronous-unwind-tables -s -Os -nostdlib -static -fno-ident \
 *     -include ../../nolibc/nolibc.h -o pause pause.c -lgcc
 */

#ifndef NOLIBC
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#endif

int main(int argc, char **argv, char **envp)
{
	int i;

	if (argc > 1 && fork() != 0)
		return 0;

	/* child */
	for (i = 0; i < 1024; i++)
		close(i);
	setsid();
	while (1) {
		select(0, NULL, NULL, NULL, NULL);
	}
}
