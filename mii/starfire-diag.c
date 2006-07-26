/* starfire-diag.c: Diagnostic and setup for the Adaptec Starfire DuraLAN.

   This is a diagnostic and EEPROM setup program for PCI adapters
   based on the following chips:
	Adaptec 6915 "Starfire"
   This file contains the complete diagnostic code.

	Copyright 1998-2000 by Donald Becker.
	This software may be used and distributed according to the terms of
	the GNU General Public License (GPL), incorporated herein by reference.
	Contact the author for use under other terms.

	This program must be compiled with "-O"!
	See the bottom of this file for the suggested compile-command.

	The author may be reached as becker@scyld.com, or C/O
	 Scyld Computing Corporation
	 410 Severn Ave., Suite 210
	 Annapolis MD 21403

	Support and updates available at
	http://www.scyld.com/diag/index.html
	http://scyld.com/expert/mii-status.html
	http://scyld.com/expert/NWay.html

	Common-sense licensing statement: Using any portion of this program in
	your own program means that you must give credit to the original author
	and release the resulting code under the GPL.
*/

static char *version_msg =
"starfire-diag.c:v2.01 7/26/2000 Donald Becker (becker@scyld.com)\n"
" http://www.scyld.com/diag/index.html\n";
static char *usage_msg =
"Usage: etherdiag [-aEefFmqrRtvVwW] [-p <IOport>] [-[AF] <media>]\n";

#if ! defined(__OPTIMIZE__)
#warning  You must compile this program with the correct options!
#warning  See the last lines of the source file.
#error You must compile this driver with "-O".
#endif
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

#if defined(__linux__)  &&  __GNU_LIBRARY__ == 1
#include <asm/io.h>			/* Newer libraries use <sys/io.h> instead. */
#else
#include <sys/io.h>
#endif

/* No libmii.h or libflash.h yet. */
extern int show_mii_details(long ioaddr, int phy_id);
extern int monitor_mii(long ioaddr, int phy_id);

extern int flash_show(long addr_ioaddr, long data_ioaddr);
extern int flash_dump(long addr_ioaddr, long data_ioaddr, char *filename);
extern int flash_program(long addr_ioaddr, long data_ioaddr, char *filename);
extern int (*flash_in_hook)(long addr, int offset);
extern void (*flash_out_hook)(long addr, int offset, int val);

/* We should use __u8 .. __u32, but they are not always defined. */
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

struct option longopts[] = {
 /* { name  has_arg  *flag  val } */
	{"Advertise", 1, 0, 'A'},
	{"base-address", 1, 0, 'p'},
	{"show_all_registers",	0, 0, 'a'},	/* Print all registers. */
	{"help",	0, 0, 'h'},	/* Give help */
	{"show-eeprom",  0, 0, 'e'}, /* Dump EEPROM contents (-ee valid). */
	{"emergency-rewrite",  0, 0, 'E'}, /* Re-write a corrupted EEPROM.  */
	{"force-detection",  0, 0, 'f'},
	{"new-interface",  1, 0, 'F'},	/* New interface (built-in, AUI, etc.) */
	{"new-hwaddr",  1, 0, 'H'},	/* Set a new hardware address. */
	{"show-mii",  0, 0, 'm'},	/* Dump MII management registers. */
	{"port-base",  1, 0, 'p'},	/* Use the specified I/O address. */
	{"quiet",	0, 0, 'q'},		/* Decrease verbosity */
	{"reset",	0, 0, 'R'},		/* Reset the transceiver. */
	{"chip-type",  1, 0, 't'},	/* Assume the specified chip type index. */
	{"test",	0, 0, 'T'},		/* Do register and SRAM test. */
	{"verbose",	0, 0, 'v'},		/* Verbose mode */
	{"version", 0, 0, 'V'},		/* Display version number */
	{"write-EEPROM", 1, 0, 'w'},/* Actually write the EEPROM with new vals */
	{ 0, 0, 0, 0 }
};

extern int starfire_diag(int vend_id, int dev_id, long ioaddr, int part_idx);

/* Chip-specific flags. Yes, it's grungy to have the enum here. */

/* The table of known chips.
   Because of the bogus /proc/pci interface we must have both the exact
   name and a PCI vendor/device IDs.
   This table is searched in order: place specific entries followed by
   'catch-all' general entries. */
struct pcidev_entry {
	char *proc_pci_name;
	char *part_name;
	int vendor, device, device_mask;
	int flags;
	int io_size;
	int (*diag_func)(int vendor_id, int device_id, long ioaddr, int part_num);
} pcidev_tbl[] = {
	{ "Adaptec Unknown device", "Adaptec Starfire 6915",
	  0x9004, 0x6915, 0xffff, 0, 256, starfire_diag},
	{ 0, 0, 0, 0},
};

int verbose = 1, opt_f = 0, debug = 0;
int show_regs = 0, show_eeprom = 0, show_mii = 0;
unsigned int opt_a = 0,					/* Show-all-interfaces flag. */
	opt_restart = 0,
	opt_reset = 0,
	opt_watch = 0,
	opt_G = 0;
unsigned int opt_GPIO = 0;		/* General purpose I/O setting. */
int do_write_eeprom = 0, do_test = 0;
int nway_advertise = 0, fixed_speed = -1;
int new_default_media = -1;
/* Valid with libflash only. */
static unsigned int opt_flash_show = 0;
static char	*opt_flash_dumpfile = NULL, *opt_flash_loadfile = NULL;

static unsigned char new_hwaddr[6], set_hwaddr = 0;

static int scan_proc_pci(int card_num);
static int parse_media_type(const char *capabilities);
static int get_media_index(const char *name);


int
main(int argc, char **argv)
{
	int port_base = 0, chip_type = 0;
	int errflag = 0, show_version = 0;
	int emergency_rewrite = 0;
	int c, longind;
	int card_num = 0;
	extern char *optarg;

	while ((c = getopt_long(argc, argv, "#:aA:DeEfF:G:mp:qrRst:vVwWH:BL:S:",
							longopts, &longind))
		   != -1)
		switch (c) {
		case '#': card_num = atoi(optarg);	break;
		case 'a': show_regs++; opt_a++;		break;
		case 'A': nway_advertise = parse_media_type(optarg); break;
		case 'D': debug++;			break;
		case 'e': show_eeprom++;	break;
		case 'E': emergency_rewrite++;	 break;
		case 'f': opt_f++;			break;
		case 'F': new_default_media = get_media_index(optarg);
			if (new_default_media < 0)
				errflag++;
			break;
		case 'G': opt_G++; opt_GPIO = strtol(optarg, NULL, 16); break;
		case 'H':
			{
				int hwaddr[6], i;
				if (sscanf(optarg, "%2x:%2x:%2x:%2x:%2x:%2x",
						   hwaddr, hwaddr + 1, hwaddr + 2,
						   hwaddr + 3, hwaddr + 4, hwaddr + 5) == 6) {
					for (i = 0; i < 6; i++)
						new_hwaddr[i] = hwaddr[i];
					set_hwaddr++;
				} else
					errflag++;
				break;
			}
		case 'm': show_mii++;	 break;
		case 'p':
			port_base = strtol(optarg, NULL, 16);
			break;
		case 'q': if (verbose) verbose--;		 break;
		case 'r': opt_restart++;	break;
		case 'R': opt_reset++;		break;
		case 't': chip_type = atoi(optarg);	break;
		case 'v': verbose++;		break;
		case 'V': show_version++;	break;
		case 'w': do_write_eeprom++;	break;
		case 'W': opt_watch++;		break;
		case 'B': opt_flash_show++;	break;
		case 'L': opt_flash_loadfile = optarg;	break;
		case 'S': opt_flash_dumpfile = optarg;	break;
		case '?':
			errflag++;
		}
	if (errflag) {
		fprintf(stderr, usage_msg);
		return 3;
	}

	if (verbose || show_version)
		printf(version_msg);

	if (chip_type < 0
		|| chip_type >= sizeof(pcidev_tbl)/sizeof(pcidev_tbl[0]) - 1) {
		int i;
		fprintf(stderr, "Valid numeric chip types are:\n");
		for (i = 0; pcidev_tbl[i].part_name; i++) {
			fprintf(stderr, "  %d\t%s\n", i, pcidev_tbl[i].part_name);
		}
		return 3;
	}

	/* Get access to all of I/O space. */
	if (iopl(3) < 0) {
		perror("Network adapter diagnostic: iopl()");
		fprintf(stderr, "This program must be run as root.\n");
		return 2;
	}

	/* Try to read a likely port_base value from /proc/pci. */
	if (port_base) {
		printf("Assuming a %s adapter at %#x.\n",
			   pcidev_tbl[chip_type].part_name, port_base);
		pcidev_tbl[chip_type].diag_func(0, 0, port_base, chip_type);
	} else if ( scan_proc_pci(card_num) == 0) {
		fprintf(stderr,
				"Unable to find a recognized card in /proc/pci.\nIf there is"
				" a card in the machine, explicitly set the I/O port"
				" address\n  using '-p <ioaddr> -t <chip_type_index>'\n"
				" Use '-t -1' to see the valid chip types.\n");
		return ENODEV;
	}

	if (show_regs == 0  &&  show_eeprom == 0  &&  show_mii == 0)
		printf(" Use '-a' or '-aa' to show device registers,\n"
			   "     '-e' to show EEPROM contents, -ee for parsed contents,\n"
			   "  or '-m' or '-mm' to show MII management registers.\n");

	return 0;
}


/* Generic (all PCI diags) code to find cards. */

static char bogus_iobase[] =
"This chip has not been assigned a valid I/O address, and will not function.\n"
" If you have warm-booted from another operating system, a complete \n"
" shut-down and power cycle may restore the card to normal operation.\n";

static char bogus_irq[] =
"This chip has not been assigned a valid IRQ, and will not function.\n"
" This must be fixed in the PCI BIOS setup.  The device driver has no way\n"
" of changing the PCI IRQ settings.\n";

static int scan_proc_bus_pci(int card_num)
{
	int card_cnt = 0, chip_idx = 0;
	int port_base;
	char buffer[514];
	unsigned int pci_bus, pci_devid, irq, pciaddr0, pciaddr1;
	int i;
	FILE *fp = fopen("/proc/bus/pci/devices", "r");

	if (fp == NULL) {
		if (debug) fprintf(stderr, "Failed to open /proc/bus/pci/devices.\n");
		return -1;
	}
	while (fgets(buffer, sizeof(buffer), fp)) {
		if (debug > 1)
			fprintf(stderr, " Parsing line -- %s", buffer);
		if (sscanf(buffer, "%x %x %x %x %x",
				   &pci_bus, &pci_devid, &irq, &pciaddr0, &pciaddr1) <= 0)
			break;
		for (i = 0; pcidev_tbl[i].vendor; i++) {
			if (pci_devid !=
				(pcidev_tbl[i].vendor << 16) + pcidev_tbl[i].device)
				continue;
			chip_idx = i;
			card_cnt++;
			/* Select the I/O address. */
			port_base = pciaddr0 & 1  ?  pciaddr0 & ~1 : pciaddr1 & ~1;
			if (card_num == 0 || card_num == card_cnt) {
				printf("Index #%d: Found a %s adapter at %#x.\n",
					   card_cnt, pcidev_tbl[chip_idx].part_name,
					   port_base);
				if (irq == 0  || irq == 255)
					printf(bogus_irq);
				if (port_base)
					pcidev_tbl[chip_idx].diag_func(0,0,port_base, i);
				else
					printf(bogus_iobase);
				break;
			}
		}
	}
	fclose(fp);
	return card_cnt;
}

static int scan_proc_pci(int card_num)
{
	int card_cnt = 0, chip_idx = 0;
	char chip_name[40];
	FILE *fp;
	int port_base;

	if ((card_cnt = scan_proc_bus_pci(card_num)) >= 0)
		return card_cnt;
	card_cnt = 0;

	fp = fopen("/proc/pci", "r");
	if (fp == NULL)
		return 0;
	{
		char buffer[514];
		int pci_bus, pci_device, pci_function, vendor_id, device_id;
		int state = 0;
		if (debug) printf("Done open of /proc/pci.\n");
		while (fgets(buffer, sizeof(buffer), fp)) {
			if (debug > 1)
				fprintf(stderr, " Parse state %d line -- %s", state, buffer);
			if (sscanf(buffer, " Bus %d, device %d, function %d",
					   &pci_bus, &pci_device, &pci_function) > 0) {
				chip_idx = 0;
				state = 1;
				continue;
			}
			if (state == 1) {
				if (sscanf(buffer, " Ethernet controller: %39[^\n]",
						   chip_name) > 0) {
					int i;
					if (debug)
						printf("Named ethernet controller %s.\n", chip_name);
					for (i = 0; pcidev_tbl[i].proc_pci_name; i++)
						if (strncmp(pcidev_tbl[i].proc_pci_name, chip_name,
									strlen(pcidev_tbl[i].proc_pci_name))
							== 0) {
							state = 2;
							chip_idx = i;
							continue;
						}
					continue;
				}
				/* Handle a /proc/pci that does not recognize the card. */
				if (sscanf(buffer, " Vendor id=%x. Device id=%x",
						   &vendor_id, &device_id) > 0) {
					int i;
					if (debug)
						printf("Found vendor 0x%4.4x device ID 0x%4.4x.\n",
							   vendor_id, device_id);
					for (i = 0; pcidev_tbl[i].vendor; i++)
						if (vendor_id == pcidev_tbl[i].vendor  &&
							(device_id & pcidev_tbl[i].device_mask)
							== pcidev_tbl[i].device)
							break;
					if (pcidev_tbl[i].vendor == 0)
						continue;
					chip_idx = i;
					state = 2;
				}
			}
			if (state == 2) {
				if (sscanf(buffer, "  I/O at %x", &port_base) > 0) {
					card_cnt++;
					state = 3;
					if (card_num == 0 || card_num == card_cnt) {
						printf("Index #%d: Found a %s adapter at %#x.\n",
							   card_cnt, pcidev_tbl[chip_idx].part_name,
							   port_base);
						if (port_base)
							pcidev_tbl[chip_idx].diag_func
								(vendor_id, device_id, port_base, chip_idx);
						else
							printf(bogus_iobase);
					}
				}
			}
		}
	}
	fclose(fp);
	return card_cnt;
}

/* Convert a text media name to a NWay capability word. */
static int parse_media_type(const char *capabilities)
{
	const char *mtypes[] = {
		"100baseT4", "100baseTx", "100baseTx-FD", "100baseTx-HD",
		"10baseT", "10baseT-FD", "10baseT-HD", 0,
	};
	int cap_map[] = { 0x0200, 0x0180, 0x0100, 0x0080, 0x0060, 0x0040, 0x0020,};
	int i;
	if (debug)
		fprintf(stderr, "Advertise string is '%s'.\n", capabilities);
	for (i = 0; mtypes[i]; i++)
		if (strcasecmp(mtypes[i], capabilities) == 0)
			return cap_map[i];
	if ((i = strtol(capabilities, NULL, 16)) <= 0xffff)
		return i;
	fprintf(stderr, "Invalid media advertisement '%s'.\n", capabilities);
	return 0;
}

/* Return the index of a valid media name.
   0x0800	Power up autosense (check speed only once)
   0x8000	Dynamic Autosense
*/
/* A table of media names to indices.  This matches the Digital Tulip
   SROM numbering, primarily because that is the most complete list.
   Other chips will have to map these number to their internal values.
*/
struct { char *name; int value; } mediamap[] = {
	{ "10baseT", 0 },
	{ "10base2", 1 },
	{ "AUI", 2 },
	{ "100baseTx", 3 },
	{ "10baseT-FDX", 0x204 },
	{ "100baseTx-FDX", 0x205 },
	{ "100baseT4", 6 },
	{ "100baseFx", 7 },
	{ "100baseFx-FDX", 8 },
	{ "MII", 11 },
	{ "Autosense", 0x0800 },
	{ 0, 0 },
};

static int get_media_index(const char *name)
{
	int i;
	for (i = 0; mediamap[i].name; i++)
		if (strcasecmp(name, mediamap[i].name) == 0)
			return i;
	if (isdigit(*name)  &&  atoi(name) >= 00)
		return atoi(name);
	fprintf(stderr, "Invalid interface specified: it must be one of\n  ");
	for (i = 0; mediamap[i].name; i++)
		fprintf(stderr, "  %s", mediamap[i].name);
	fprintf(stderr, ".\n");
	return -1;
}


/* Chip-specific section. */

/* The chip-specific section for the Adaptec Starfire diagnostic. */

static int read_eeprom(long ioaddr, int location);
int mdio_read(long ioaddr, int phy_id, int location);
void mdio_write(long ioaddr, int phy_id, int location, int value);

/* Offsets to the various registers.
   All accesses need not be longword aligned. */
enum starfire_offsets {
	GenCtrl=0x50070,
	IntrClear=0x50084, IntrStatus=0x50084, IntrEnable=0x50088,
	MIICtrl=0x52000, StationAddr=0x50120, EEPROMCtrl=0x51000,
	TxRingPtr=0x50098, HiPriTxRingPtr=0x50094, /* Low and High priority. */
	TxRingAddrHi=0x5009C,		/* 64 bit address extension. */
	TxProducerIdx=0x500A0, TxConsumerIdx=0x500A4,
	TxThreshold=0x500B0, TxMode=0x55000,
};

/* Non-interrupting events. */
const char *event_names[16] = {
	"Tx Abort", "Rx frame complete", "Transmit done",
};
struct config_name { int val, mask; const char*name;}
static rcvr_mode[] = {
	{0x44, 0xff, "Normal unicast and perfect multicast filtering"},
	{0x34, 0xff, "Normal unicast and hashed multicast"},
	{0x03, 0x02, "Normal unicast and all multicast"},
	{0x01, 0x01, "Promiscuous"},
	{0x00, 0x00, "Unknown/invalid"},
};

/* Values read from the EEPROM, and the new image. */
#define EEPROM_SIZE 32
unsigned int eeprom_contents[EEPROM_SIZE];
unsigned int new_ee_contents[EEPROM_SIZE];

int starfire_diag(int vendor_id, int device_id, long ioaddr, int part_idx)
{
	int i;

	if (verbose || show_regs) {
		int MAC_config, MAC_addr;
		int rx_mode = inl(ioaddr + 0xf4);
		int pci_stats = inl(ioaddr + 0x48);
		if (opt_a) {
			printf("%s chip registers at %#lx",
				   pcidev_tbl[part_idx].part_name, ioaddr);
			for (i = 0; i < pcidev_tbl[part_idx].io_size; i += 4) {
				if ((i & 0x1f) == 0)
					printf("\n   0x%3.3X:", i);
				if (!opt_f  &&  i == 0x80)
					printf(" **INTR**");
				else
					printf(" %8.8x", inl(ioaddr + i));
			}
			printf("\n");
		}
		printf(" Queue states\n"
			   "   Tx producer index %d, consumer index %d.\n"
			   "   Rx consumer index %d, producer index %d.\n",
			   inw(ioaddr + 0xa0), inw(ioaddr + 0xa4),
			   inw(ioaddr + 0xea), inw(ioaddr + 0xe8));
		printf(" PCI statistics\n");
		printf("  PCI Bus maximum latency was %d nsec, peak interrupt latency "
			   "%d usec.\n",
			   (pci_stats>>24)*480, (pci_stats & 0xff) *30);
		printf("  PCI accesses: %d slave cycles, %d master bursts.\n",
			   inl(ioaddr + 0x48) & 0xffff, inl(ioaddr + 0x4C) & 0xffff);
		printf(" Interrupts pending: %8.8x\n", inl(ioaddr + 0x84));
		for (i = 0; rcvr_mode[i].mask; i++)
			if ((rx_mode & rcvr_mode[i].mask) == rcvr_mode[i].val) break;
		printf(" Receive mode is 0x%8.8x: %s.\n", rx_mode, rcvr_mode[i].name);
		outl(0x50120, ioaddr + 0x68);
		MAC_addr = inl(ioaddr + 0x6c);
		printf(" MAC address is --:--:%2.2x:%2.2x:%2.2x:%2.2x.\n",
			   (MAC_addr >> 24) & 0xff, (MAC_addr >> 16) & 0xff,
			   (MAC_addr >>  8) & 0xff, (MAC_addr >>  0) & 0xff);
		outl(0x55000, ioaddr + 0x68);
		MAC_config = inl(ioaddr + 0x6c);
		printf(" MAC Config 1 is %8.8x: %s-duplex.\n", MAC_config,
			   MAC_config & 0x02 ? "full" : "half");
		if (opt_a > 2) {
			printf("  Statistics registers: ");
			for (i = 0; i < 0x100; i+=4) {
				outl(0x57000+i, ioaddr + 0x68);
				printf(" %8.8x%s", inl(ioaddr + 0x6c),
					   i%16 == 15 ? "\n  " : "");
			}
			printf("\n");
		}
	}

	for (i = 0; i < EEPROM_SIZE; i++)
		eeprom_contents[i] = read_eeprom(ioaddr, i);

	if (show_eeprom) {
		u32 sum = 0;
		u8 *ee = (u8*)eeprom_contents;
		if (show_eeprom > 1) {
			printf("EEPROM contents:");
			for (i = 0; i < EEPROM_SIZE; i++) {
				if ((i & 7) == 0)
					printf("\n0x%3.3x: ", i);
				printf(" %8.8x", eeprom_contents[i]);
			}
			printf("\n");
		}
		printf("EEPROM Subsystem IDs, Vendor %2.2x%2.2x Device %2.2x%2.2x.\n",
			   ee[7], ee[6], ee[9], ee[8]);
		printf("EEPROM Station address is ");
		for (i = 20; i > 15; i--)
			printf("%2.2x:", ee[i]);
		printf("%2.2x.\n", ee[i]);
		for (i = 0; i < EEPROM_SIZE-1; i++)
			sum ^= eeprom_contents[i];
		printf("EEPROM Checksum is %8.8x.\n", sum);
		for (i = 0, sum=0; i < EEPROM_SIZE*4 - 2; i++)
			sum += ee[i];
		printf("EEPROM Checksum is %8.8x.\n", sum);
	}
	if (show_mii) {
		int phys[4], phy, phy_idx = 0;
		for (phy = 0; phy < 32 && phy_idx < 4; phy++) {
			int mii_status = mdio_read(ioaddr, phy, 1);
			if (mii_status != 0xffff  &&
				mii_status != 0x0000) {
				phys[phy_idx++] = phy;
				printf(" MII PHY found at address %d, status 0x%4.4x.\n",
					   phy, mii_status);
			}
		}
		if (phy_idx == 0)
			printf(" ***WARNING***: No MII transceivers found!\n");
		for (phy = 0; phy < phy_idx; phy++) {
			int mii_reg;
			printf("  MII PHY #%d transceiver registers:", phys[phy]);
			for (mii_reg = 0; mii_reg < 32; mii_reg++)
				printf("%s %4.4x", (mii_reg % 8) == 0 ? "\n   " : "",
					   mdio_read(ioaddr, phys[phy], mii_reg));
			printf(".\n");
		}
#ifdef LIBMII
		show_mii_details(ioaddr, phys[0]);
		if (show_mii > 1)
			monitor_mii(ioaddr, phys[0]);
#endif
	}

	return 0;
}

static int read_eeprom(long ioaddr, int location)
{
	outl(EEPROMCtrl + location*4, ioaddr + 0x68);
	return inl(ioaddr + 0x6C);
}

#ifdef notused
static void write_eeprom(long ioaddr, int location, int value)
{
	outl(EEPROMCtrl + location*4, ioaddr + 0x68);
	outl(value, ioaddr + 0x6C);
	return;
}
#endif

/* MII Managemen Data I/O accesses.
   These routines assume the MDIO controller is idle, and do not exit until
   the command is finished. */

int mdio_read(long ioaddr, int phy_id, int location)
{
	int result, boguscnt=1000;

	outl(MIICtrl + (phy_id<<7) + location*4, ioaddr + 0x68);
	do
		result = inl(ioaddr + 0x6C);
	while ((result & 0xC0000000) != 0x80000000 && --boguscnt >= 0);
	return result & 0xffff;
}

void mdio_write(long ioaddr, int phy_id, int location, int value)
{
	outl(MIICtrl + (phy_id<<7) + location*4, ioaddr + 0x68);
	outl(value, ioaddr + 0x6C);
	return;
}

/*
 * Local variables:
 *  compile-command: "cc -O -Wall -o starfire-diag starfire-diag.c"
 *  tab-width: 4
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 */
