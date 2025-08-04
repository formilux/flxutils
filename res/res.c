/* returns 0 on success, 1 on failed cmdline, 2 on resolution failure.
 * only prints the resolution.
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <netdb.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

int quiet = 0;

void alarm_handler(int signum __attribute__((unused)))
{
	if (!quiet)
		fprintf(stderr, "Timeout\n");
	exit(2);
}

int main(int argc, char **argv)
{
	char addrstr[INET6_ADDRSTRLEN];
	struct in_addr ipv4addr;
	struct in6_addr ipv6addr;
	struct hostent *hostent;
	int n2a = 0;

	if (argc > 1 && strcmp(argv[1], "-q") == 0) {
		quiet = 1;
		argc--;
		argv++;
	}

	if (argc != 2) {
		fprintf(stderr, "Usage: %s [-q] <ip-address-or-name-to-resolve>\n", argv[0]);
		fprintf(stderr, "Returns 0 (success), 1 (cmdline), 2 (res failure)\n");
		return 1;
	}

	signal(SIGALRM, alarm_handler);
	alarm(1);

	/* address to name ? */
	if (inet_pton(AF_INET, argv[1], &ipv4addr) == 1)
		hostent = gethostbyaddr(&ipv4addr, sizeof(ipv4addr), AF_INET);
	else if (inet_pton(AF_INET6, argv[1], &ipv6addr) == 1)
		hostent = gethostbyaddr(&ipv6addr, sizeof(ipv6addr), AF_INET6);
	else {
		/* doesn't parse as an address, let's try name to address */
		hostent = gethostbyname(argv[1]);
		n2a = 1;
	}

	if (hostent == NULL) {
		if (!quiet)
			fprintf(stderr, "%s lookup of %s failed\n", n2a ? "Direct" : "Reverse", argv[1]);
		return 2;
	}

	if (!n2a) {
		fprintf(stdout, "%s\n", hostent->h_name);
	}
	else {
		if ((hostent->h_addrtype == AF_INET &&
		     inet_ntop(AF_INET, hostent->h_addr_list[0], addrstr, sizeof(addrstr)) == NULL) ||
		    (hostent->h_addrtype == AF_INET6 &&
		     inet_ntop(AF_INET6, hostent->h_addr_list[0], addrstr, sizeof(addrstr)) == NULL)) {
			if (!quiet)
				fprintf(stderr, "Unknown address family for %s\n", argv[1]);
			return 2;
		}
		fprintf(stdout, "%s\n", addrstr);
	}

	return 0;
}
