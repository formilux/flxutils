/* Mode: C;
 * mii-diag.c: Examine and set the MII registers of a network interfaces.

	Usage:	mii-diag [-vw] interface.

	Notes:
	The compile-command is at the end of this source file.
	This program only works with drivers that implement MII ioctl() calls.

	Written/copyright 1997-2001 by Donald Becker <becker@scyld.com>

	This program is free software; you can redistribute it
	and/or modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation.

	The author may be reached as becker@scyld.com, or C/O
	 Scyld Computing Corporation
	 410 Severn Ave., Suite 210
	 Annapolis MD 21403

	References
	http://scyld.com/expert/mii-status.html
	http://scyld.com/expert/NWay.html
	http://www.national.com/pf/DP/DP83840.html
*/

static char version[] =
"mii-diag.c:v2.02 5/21/2001 Donald Becker (becker@scyld.com)\n"
" http://www.scyld.com/diag/index.html\n";

static const char usage_msg[] =
"Usage: mii-diag [-aDfrRvVw] [-AF <to-advertise>] [--watch] <interface>.\n";

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#ifdef use_linux_libc5
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#endif

typedef unsigned short u16;
typedef unsigned int u32;

//#ifndef SIOCGMIIPHY
#define SIOCGMIIPHY (SIOCDEVPRIVATE)		/* Get the PHY in use. */
#define SIOCGMIIREG (SIOCDEVPRIVATE+1) 		/* Read a PHY register. */
#define SIOCSMIIREG (SIOCDEVPRIVATE+2) 		/* Write a PHY register. */
#define SIOCGPARAMS (SIOCDEVPRIVATE+3) 		/* Read operational parameters. */
#define SIOCSPARAMS (SIOCDEVPRIVATE+4) 		/* Set operational parameters. */
//#endif

struct option longopts[] = {
 /* { name  has_arg  *flag  val } */
    {"all-interfaces", 0, 0, 'a'},	/* Show all interfaces. */
	{"Advertise",	1, 0, 'A'},		/* Change the capabilities advertised. */
    {"debug",       0, 0, 'D'},		/* Increase the debug level. */
    {"force",       0, 0, 'f'},		/* Force the operation. */
    {"parameters",  0, 0, 'G'},		/* Write general settings value. */
    {"phy",			1, 0, 'p'},		/* Set the PHY (MII address) to report. */
    {"restart",		0, 0, 'r'},		/* Restart the link negotiation */
    {"Reset",		0, 0, 'R'},		/* Reset the transceiver. */
    {"status",		0, 0, 's'},		/* Non-zero exit status w/ no link beat. */
    {"help", 		0, 0, '?'},		/* Give help */
    {"verbose", 	0, 0, 'v'},		/* Report each action taken.  */
    {"version", 	0, 0, 'V'},		/* Emit version information.  */
    {"watch", 		0, 0, 'w'},		/* Constantly monitor the port.  */
    { 0, 0, 0, 0 }
};

/* Usually in libmii.c, but trivial substitions are below. */
extern int  show_mii_details(long ioaddr, int phy_id);
extern void monitor_mii(long ioaddr, int phy_id);

/* Command-line flags. */
unsigned int opt_a = 0,					/* Show-all-interfaces flag. */
	opt_f = 0,					/* Force the operation. */
	opt_G = 0, opt_G_value = 0,
	verbose = 0,				/* Verbose flag. */
	debug = 0,
	opt_version = 0,
	opt_restart = 0,
	opt_reset = 0,
	opt_status = 0,
	opt_watch = 0;
static int nway_advertise = -1;
static int fixed_speed = -1;
static int override_phy = -1;
int skfd = -1;					/* AF_INET socket for ioctl() calls.	*/
struct ifreq ifr;

int do_one_xcvr(int skfd);
int show_basic_mii(long ioaddr, int phy_id);
int mdio_read(int skfd, int phy_id, int location);
void mdio_write(int skfd, int phy_id, int location, int value);
static int parse_advertise(const char *capabilities);


int
main(int argc, char **argv)
{
	int c, errflag = 0;
	char **spp, *ifname;

	while ((c = getopt_long(argc, argv, "aA:DfF:G:p:rRsvVw?", longopts, 0)) != EOF)
		switch (c) {
		case 'a': opt_a++; break;
		case 'A': nway_advertise = parse_advertise(optarg); break;
		case 'D': debug++;			break;
		case 'f': opt_f++; break;
		case 'F': fixed_speed = parse_advertise(optarg); break;
		case 'G': opt_G++; opt_G_value = atoi(optarg); break;
		case 'p': override_phy = atoi(optarg); break;
		case 'r': opt_restart++;	break;
		case 'R': opt_reset++;		break;
		case 's': opt_status++;		break;
		case 'v': verbose++;		break;
		case 'V': opt_version++;	break;
		case 'w': opt_watch++;		break;
		case '?': errflag++;
		}
	if (errflag) {
		fprintf(stderr, usage_msg);
		return 2;
	}

	if (verbose || opt_version)
		printf(version);

	/* Open a basic socket. */
	if ((skfd = socket(AF_INET, SOCK_DGRAM,0)) < 0) {
		perror("socket");
		exit(-1);
	}

	if (debug)
		fprintf(stderr, "DEBUG: argc=%d, optind=%d and argv[optind] is %s.\n",
				argc, optind, argv[optind]);

	/* No remaining args means show all interfaces. */
	if (optind == argc) {
		ifname = "eth0";
		fprintf(stderr, "Using the default interface 'eth0'.\n");
	} else {
		/* Copy the interface name. */
		spp = argv + optind;
		ifname = *spp++;
	}

	if (ifname == NULL) {
		ifname = "eth0";
		fprintf(stderr, "Using the default interface 'eth0'.\n");
	}

	/* Get the vitals from the interface. */
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(skfd, SIOCGMIIPHY, &ifr) < 0) {
		fprintf(stderr, "SIOCGMIIPHY on %s failed: %s\n", ifname,
				strerror(errno));
		(void) close(skfd);
		return 1;
	}

	do_one_xcvr(skfd);

	(void) close(skfd);
	return 0;
}

int do_one_xcvr(int skfd)
{
	u16 *data = (u16 *)(&ifr.ifr_data);
	unsigned phy_id = data[0];

	if (override_phy >= 0) {
		printf("Using the specified MII PHY index %d.\n", override_phy);
		phy_id = override_phy;
	}

	if (opt_G) {
		/* Set an undefined general-purpose value.
		   Modify this code section to do special hacks such as
		   mdio_write(skfd, phy_id, 19, opt_G_value);
		*/
		u32 *data = (u32 *)(&ifr.ifr_data);
		data[0] = opt_G_value;
		if (ioctl(skfd, SIOCSPARAMS, &ifr) < 0) {
			fprintf(stderr, "SIOCSPARAMS on %s failed: %s\n", ifr.ifr_name,
					strerror(errno));
			return -1;
		}
	}

	if (opt_reset) {
		printf("Resetting the transceiver...\n");
		mdio_write(skfd, phy_id, 0, 0x8000);
	}
	/* Note: PHY addresses > 32 are pseudo-MII devices, usually built-in. */
	if (phy_id < 64  &&  nway_advertise >= 0) {
		printf(" Setting the media capability advertisement register of "
			   "PHY #%d to 0x%4.4x.\n", phy_id, nway_advertise | 1);
		mdio_write(skfd, phy_id, 4, nway_advertise | 1);
		mdio_write(skfd, phy_id, 0, 0x1000);
	}

	if (opt_restart) {
		printf("Restarting negotiation...\n");
		mdio_write(skfd, phy_id, 0, 0x0000);
		mdio_write(skfd, phy_id, 0, 0x1200);
	}
	/* To force 100baseTx-HD do  mdio_write(skfd, phy_id, 0, 0x2000); */
	if (fixed_speed >= 0) {
		int reg0_val = 0;
		if (fixed_speed & 0x0180) 		/* 100mpbs */
			reg0_val |=  0x2000;
		if (fixed_speed & 0x0140)		/* Full duplex */
			reg0_val |= 0x0100;
		printf("Setting the speed to \"fixed\", Control register %4.4x.\n",
			   reg0_val);
		mdio_write(skfd, phy_id, 0, reg0_val);
	}

	show_basic_mii(skfd, phy_id);
#ifdef LIBMII
	if (verbose)
		show_mii_details(skfd, phy_id);
#else
	if (verbose || debug) {
		int mii_reg, mii_val;
		printf(" MII PHY #%d transceiver registers:", phy_id);
		for (mii_reg = 0; mii_reg < 32; mii_reg++) {
			mii_val = mdio_read(skfd, phy_id, mii_reg);
			printf("%s %4.4x", (mii_reg % 8) == 0 ? "\n  " : "",
				   mii_val);
		}
		printf("\n");
	}
#endif

	if (opt_watch)
		monitor_mii(skfd, phy_id);
	if (opt_status &&
		(mdio_read(skfd, phy_id, 1) & 0x0004) == 0)
		exit(2);
	return 0;
}

int mdio_read(int skfd, int phy_id, int location)
{
	u16 *data = (u16 *)(&ifr.ifr_data);

	data[0] = phy_id;
	data[1] = location;

	if (ioctl(skfd, SIOCGMIIREG, &ifr) < 0) {
		fprintf(stderr, "SIOCGMIIREG on %s failed: %s\n", ifr.ifr_name,
				strerror(errno));
		return -1;
	}
	return data[3];
}

void mdio_write(int skfd, int phy_id, int location, int value)
{
	u16 *data = (u16 *)(&ifr.ifr_data);

	data[0] = phy_id;
	data[1] = location;
	data[2] = value;

	if (ioctl(skfd, SIOCSMIIREG, &ifr) < 0) {
		fprintf(stderr, "SIOCSMIIREG on %s failed: %s\n", ifr.ifr_name,
				strerror(errno));
	}
}

/* Parse the command line argument for advertised capabilities. */
static int parse_advertise(const char *capabilities)
{
	const char *mtypes[] = {
		"100baseT4", "100baseTx", "100baseTx-FD", "100baseTx-HD",
		"10baseT", "10baseT-FD", "10baseT-HD", 0,
	};
	int cap_map[] = { 0x0200, 0x0180, 0x0100, 0x0080, 0x0060, 0x0040, 0x0020,};
	int i;
	if ( ! capabilities) {
		fprintf(stderr, "You passed -A 'NULL'.  You must provide a media"
				" list to advertise!\n");
		return -1;
	}
	if (debug)
		fprintf(stderr, "Advertise string is '%s'.\n", capabilities);
	for (i = 0; mtypes[i]; i++)
		if (strcasecmp(mtypes[i], capabilities) == 0)
			return cap_map[i];
	if ((i = strtol(capabilities, NULL, 16)) <= 0xffff)
		return i;
	fprintf(stderr, "Invalid media advertisement '%s'.\n", capabilities);
	return -1;
}

/* Trivial versions if we don't link against libmii.c */
static const char *media_names[] = {
	"10baseT", "10baseT-FD", "100baseTx", "100baseTx-FD", "100baseT4",
	"Flow-control", 0,
};
/* Various non-good bits in the command register. */
static const char *bmcr_bits[] = {
	"  Internal Collision-Test enabled!\n", "",		/* 0x0080,0x0100 */
	"  Restarted auto-negotiation in progress!\n",
	"  Transceiver isolated from the MII!\n",
	"  Transceiver powered down!\n", "", "",
	"  Transceiver in loopback mode!\n",
	"  Transceiver currently being reset!\n",
};

int show_basic_mii(long ioaddr, int phy_id)
{
	int mii_reg, i;
	u16 mii_val[32];
	u16 bmcr, bmsr, new_bmsr, nway_advert, lkpar;

	for (mii_reg = 0; mii_reg < 8; mii_reg++)
		mii_val[mii_reg] = mdio_read(ioaddr, phy_id, mii_reg);
	if ( ! verbose) {
		printf("Basic registers of MII PHY #%d: ", phy_id);
		for (mii_reg = 0; mii_reg < 8; mii_reg++)
			printf(" %4.4x", mii_val[mii_reg]);
		printf(".\n");
	}

	if (mii_val[0] == 0xffff) {
		printf("  No MII transceiver present!.\n");
		return -1;
	}
	/* Descriptive rename. */
	bmcr = mii_val[0];
	bmsr = mii_val[1];
	nway_advert = mii_val[4];
	lkpar = mii_val[5];

	if (lkpar & 0x4000) {
		int negotiated = nway_advert & lkpar & 0x3e0;
		int max_capability = 0;
		/* Scan for the highest negotiated capability, highest priority
		   (100baseTx-FDX) to lowest (10baseT-HDX). */
		int media_priority[] = {8, 9, 7, 6, 5}; 	/* media_names[i-5] */
		printf(" The autonegotiated capability is %4.4x.\n", negotiated);
		for (i = 0; media_priority[i]; i++)
			if (negotiated & (1 << media_priority[i])) {
				max_capability = media_priority[i];
				break;
			}
		if (max_capability)
			printf("The autonegotiated media type is %s.\n",
				   media_names[max_capability - 5]);
		else
			printf("No common media type was autonegotiated!\n"
				   "This is extremely unusual and typically indicates a "
				   "configuration error.\n" "Perhaps the advertised "
				   "capability set was intentionally limited.\n");
	}
	printf(" Basic mode control register 0x%4.4x:", bmcr);
	if (bmcr & 0x1000)
		printf(" Auto-negotiation enabled.\n");
	else
		printf(" Auto-negotiation disabled, with\n"
			   " Speed fixed at 10%s mbps, %s-duplex.\n",
			   bmcr & 0x2000 ? "0" : "",
			   bmcr & 0x0100 ? "full":"half");
	for (i = 0; i < 9; i++)
		if (bmcr & (0x0080<<i))
			printf(bmcr_bits[i]);

	new_bmsr = mdio_read(ioaddr, phy_id, 1);
	if ((bmsr & 0x0016) == 0x0004)
		printf( " You have link beat, and everything is working OK.\n");
	else
		printf(" Basic mode status register 0x%4.4x ... %4.4x.\n"
			   "   Link status: %sestablished.\n",
			   bmsr, new_bmsr,
			   bmsr & 0x0004 ? "" :
			   (new_bmsr & 0x0004) ? "previously broken, but now re" : "not ");
	if (verbose) {
		printf("   This transceiver is capable of ");
		if (bmsr & 0xF800) {
			for (i = 15; i >= 11; i--)
				if (bmsr & (1<<i))
					printf(" %s", media_names[i-11]);
		} else
			printf("<Warning! No media capabilities>");
		printf(".\n");
		printf("   %s to perform Auto-negotiation, negotiation %scomplete.\n",
			   bmsr & 0x0008 ? "Able" : "Unable",
			   bmsr & 0x0020 ? "" : "not ");
	}

	if (bmsr & 0x0010)
		printf(" Remote fault detected!\n");
	if (bmsr & 0x0002)
		printf("   *** Link Jabber! ***\n");

	if (lkpar & 0x4000) {
		printf(" Your link partner advertised %4.4x:",
			   lkpar);
		for (i = 5; i >= 0; i--)
			if (lkpar & (0x20<<i))
				printf(" %s", media_names[i]);
		printf("%s.\n", lkpar & 0x0400 ? ", w/ 802.3X flow control" : "");
	} else if (lkpar & 0x00A0)
		printf(" Your link partner is generating %s link beat  (no"
			   " autonegotiation).\n",
			   lkpar & 0x0080 ? "100baseTx" : "10baseT");
	else if ( ! (bmcr & 0x1000))
		printf(" Link partner information is not exchanged when in"
			   " fixed speed mode.\n");
	else if ( ! (new_bmsr & 0x004))
							;	/* If no partner, do not report status. */
	else if (lkpar == 0x0001  ||  lkpar == 0x0000) {
		printf(" Your link partner does not do autonegotiation, and this "
			   "transceiver type\n  does not report the sensed link "
			   "speed.\n");
	} else
		printf(" Your link partner is strange, status %4.4x.\n", lkpar);

	printf("   End of basic transceiver information.\n\n");
	return 0;
}

#ifndef LIBMII
extern void monitor_mii(long ioaddr, int phy_id)
{
	fprintf(stderr, "\nThis version of 'mii-diag' has not been compiled with "
			"the libmii.c library \n  required for the media monitor option.\n");
}
#endif /* not compiled for LIBMII */

/*
 * Local variables:
 *  version-control: t
 *  kept-new-versions: 5
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 *  compile-command: "gcc -Wall -Wstrict-prototypes -O mii-diag.c -DLIBMII libmii.c -o mii-diag"
 *  simple-compile-command: "gcc mii-diag.c -o mii-diag"
 * End:
 */
