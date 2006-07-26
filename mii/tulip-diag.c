/* tulip-diag.c: Diagnostic and setup for Digital DC21x4*-based ethercards.

   This is a diagnostic and EEPROM setup program for PCI Ethernet adapters
   based on the following chips:
	Intel (nee Digital) "Tulip" series chips: 21040/041/140/142/143/145,
	Work-alikes from Lite-On, LinkSys, Macronix, ASIX, Compex, STmicro,
	ADMtek, Davicom and (ugh) Xircom.

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
"tulip-diag.c:v2.08 5/15/2001 Donald Becker (becker@scyld.com)\n"
" http://www.scyld.com/diag/index.html\n";
static char *usage_msg =
"Usage: tulip-diag [-aEefFmqrRtvVwW] [-p <IOport>] [-[AF] <media>]\n";

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
#include <strings.h>
#include <errno.h>

#include <asm/types.h>
#include <asm/unaligned.h>

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
	{"verbose",	0, 0, 'v'},		/* Verbose mode */
	{"version", 0, 0, 'V'},		/* Display version number */
	{"write-EEPROM", 1, 0, 'w'},/* Actually write the EEPROM with new vals */
	{ 0, 0, 0, 0 }
};

extern int   tulip_diag(int vend_id, int dev_id, long ioaddr, int part_idx);

enum tulip_flags {
	DC21041_EEPROM=1, DC21040_EEPROM=2, DC21140_EEPROM=4, PNIC_EEPROM=8,
	CSR12_IS_GPIO=0x10, PNIC_MTABLE=0x20,
	DC21143_MII=0x100, PNIC_MII=0x200, COMET_MII=0x400,	HAS_NWAY=0x800,
	ASIX_MACADDR=0x1000, COMET_MACADDR=0x2000, PNIC2_MACADDR=0x4000,
	IS_CONEXANT=0x8000, COMET_HPNA=0x10000, IS_DAVICOM=0x20000,
};

/* The table of known chips.
   Because of the bogus /proc/pci interface we must have both the exact
   name from the kernel, a common name and the PCI vendor/device IDs.
   This table is searched in order: place specific entries followed by
   'catch-all' general entries. */
struct pcidev_entry {
	const char *part_name;
	const char *proc_pci_name;
	int vendor, device, device_mask;
	int flags;
	int io_size;
	int (*diag_func)(int vendor_id, int device_id, long ioaddr, int part_idx);
} pcidev_tbl[] = {
  { "Digital Tulip series", "Digital Tulip, unknown type",
	0x1011, 0x0000, 0xffff, 0, 128, tulip_diag },
  { "Digital DC21040 Tulip", "DEC DC21040",
	0x1011, 0x0002, 0xffff, DC21040_EEPROM, 128, tulip_diag },
  { "Digital DC21041 Tulip", "DEC DC21041",
	0x1011, 0x0014, 0xffff, DC21041_EEPROM | HAS_NWAY, 128, tulip_diag },
  { "Digital DS21140 Tulip", "DEC DC21140",
	0x1011, 0x0009, 0xffff, CSR12_IS_GPIO, 128, tulip_diag },
  { "Digital DS21143 Tulip", "DEC DC21142",
	0x1011, 0x0019, 0xffff, DC21143_MII | HAS_NWAY, 128, tulip_diag },
  { "Intel 21145 Tulip", 0,
	0x8086, 0x0039, 0xffff, DC21143_MII | HAS_NWAY, 128, tulip_diag },
  { "Lite-On 82c168 PNIC", "Lite-on LNE100TX",
	0x11AD, 0x0002, 0xffff, CSR12_IS_GPIO|PNIC_EEPROM|PNIC_MII, 256, tulip_diag },
  { "Lite-On PNIC-II", 0, 0x11AD, 0xc115, 0xffff,
	PNIC_MTABLE | HAS_NWAY | PNIC2_MACADDR, 256, tulip_diag },
  { "Macronix 98713 PMAC", "Macronix MX98713",
	0x10d9, 0x0512, 0xffff, CSR12_IS_GPIO, 256, tulip_diag },
  { "Macronix 98715 PMAC", "Macronix MX98715 / MX98725",
	0x10d9, 0x0531, 0xffff, 0, 256, tulip_diag },
  { "Macronix MX98715 / MX98725", "Macronix 98725 PMAC",
	0x10d9, 0x0531, 0xffff, 0, 256, tulip_diag },
  { "ASIX AX88140", 0,
	0x125B, 0x1400, 0xffff, CSR12_IS_GPIO | ASIX_MACADDR, 128, tulip_diag },
  { "Compex TX9881", "Compex TX9881",
	0x11F6, 0x9881, 0xffff, 0, 128, tulip_diag },
  { "ADMtek AL981 Comet", 0,
	0x1317, 0x0981, 0xffff, COMET_MII | COMET_MACADDR, 256, tulip_diag },
  { "ADMtek AL985 Centaur-P", 0,
	0x1317, 0x0985, 0xffff, COMET_MACADDR, 256, tulip_diag },
  { "ADMtek AL985 Centaur-C", 0,
	0x1317, 0x1985, 0xffff, COMET_MACADDR, 256, tulip_diag },
  { "ADMtek Comet-II", 0,
	0x1317, 0x9511, 0xffff, COMET_MACADDR | COMET_HPNA, 256, tulip_diag },
  { "ADMtek AL985 Centaur (Linksys CardBus v2)", 0,
	0x13d1, 0xab02, 0xffff, COMET_MACADDR, 256, tulip_diag },
  { "ADMtek AL985 Centaur (Linksys CardBus)", 0,
	0x13d1, 0xab03, 0xffff, COMET_MACADDR, 256, tulip_diag },
  { "STMicro STE10/100 Comet", 0,
	0x104a, 0x0981, 0xffff, COMET_MACADDR, 256, tulip_diag },
  { "STMicro STE10/100A Comet", 0,
	0x104a, 0x2774, 0xffff, COMET_MACADDR, 256, tulip_diag },
  { "Accton EN1217/EN2242 (ADMtek Comet)", 0,
	0x1113, 0x1216, 0xffff, COMET_MACADDR, 256, tulip_diag },
  { "Davicom DM9102", 0,
	0x1282, 0x9102, 0xffff, CSR12_IS_GPIO, 128, tulip_diag },
  { "Davicom DM9100", 0,
	0x1282, 0x9100, 0xffff, CSR12_IS_GPIO, 128, tulip_diag },
  { "Davicom DM9100 series (unknown model)", 0,
	0x1282, 0x9100, 0xfff0, CSR12_IS_GPIO | IS_DAVICOM, 128, tulip_diag },
  { "Conexant LANfinity", 0,
	0x14f1, 0x1803, 0xffff, IS_CONEXANT, 256, tulip_diag },
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
static int emergency_rewrite = 0;

static int scan_proc_pci(int card_num);
static int parse_media_type(const char *capabilities);
static int get_media_index(const char *name);

int
main(int argc, char **argv)
{
	int port_base = 0, chip_type = 0;
	int errflag = 0, show_version = 0;
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
		case 'G': opt_G++; opt_GPIO = strtoul(optarg, NULL, 16); break;
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
			port_base = strtoul(optarg, NULL, 16);
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
" of changing the PCI IRQ settings.\n"
" See  http://www.scyld.com/expert/irq-conflict.html  for more information.\n";

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
			if ((pci_devid >> 16) != pcidev_tbl[i].vendor
				|| (pci_devid & pcidev_tbl[i].device_mask) !=
				pcidev_tbl[i].device)
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
					for (i = 0; pcidev_tbl[i].part_name; i++)
						if (pcidev_tbl[i].proc_pci_name  &&
							strncmp(pcidev_tbl[i].proc_pci_name, chip_name,
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
	if ((i = strtoul(capabilities, NULL, 16)) <= 0xffff)
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
	{ "HomePNA", 18 },
	{ "Autosense", 0x0800 },
	{ 0, 0 },
};

static int get_media_index(const char *name)
{
	int i;
	for (i = 0; mediamap[i].name; i++)
		if (strcasecmp(name, mediamap[i].name) == 0) {
			if (debug)
				fprintf(stderr, "Using media index %d for '%s'.\n", i, name);
			return i;
		}
	if (name  &&  atoi(name) >= 00)
		return atoi(name);
	fprintf(stderr, "Invalid interface specified: it must be one of\n  ");
	for (i = 0; mediamap[i].name; i++)
		fprintf(stderr, "  %s", mediamap[i].name);
	fprintf(stderr, ".\n");
	return -1;
}


/* Chip-specific section. */

int tulip_diag(int vendor_id, int device_id, long ioaddr, int part_idx);
int mdio_read(long ioaddr, int phy_id, int location);
void mdio_write(long ioaddr, int phy_id, int location, int value);
static void mdio_sync(long ioaddr);
static void setup_nway_xcvr(long ioaddr);
static void tulip_eeprom(long ioaddr, int flags);
static int read_eeprom(long ioaddr, int location, int addr_len);
static void parse_eeprom(unsigned char *ee_data, int part_idx);
static void liteon_eeprom(unsigned char *ee_data, int part_idx);
static void admtek_eeprom(unsigned char *ee_data, int part_idx);
static void conexant_eeprom(unsigned char *ee_data, int part_idx);
static void davicom_eeprom(unsigned char *ee_data, int part_idx);
static unsigned int calculate_checksum1(u16 *eeprom_contents, int len);
static unsigned int ether_crc_le(void *ptr, int length);
int do_update(long ioaddr, unsigned short *ee_values, int index,
			  char *field_name, int new_value);
static void check_for_intel_cb(long ioaddr, unsigned short *eeprom_contents);


/* Offsets to the Command and Status Registers, "CSRs".  All accesses
   must be longword instructions and quadword aligned.
   I know these are not descriptive, but they are the commonly used names
   for the Tulip design.
*/
enum tulip_offsets {
	CSR0=0,    CSR1=0x08, CSR2=0x10, CSR3=0x18, CSR4=0x20, CSR5=0x28,
	CSR6=0x30, CSR7=0x38, CSR8=0x40, CSR9=0x48, CSR10=0x50, CSR11=0x58,
	CSR12=0x60, CSR13=0x68, CSR14=0x70, CSR15=0x78 };

static const char *tx_state[8] = {
  "Stopped", "Reading a Tx descriptor", "Waiting for Tx to finish",
  "Loading Tx FIFO", "<invalid Tx state>", "Processing setup information",
  "Idle", "Closing Tx descriptor" };
static const char *rx_state[8] = {
  "Stopped", "Reading a Rx descriptor", "Waiting for Rx to finish",
  "Waiting for packets", "Suspended -- no Rx buffers",
  "Closing Rx descriptor",
  "Unavailable Rx buffer -- Flushing Rx frame",
  "Transferring Rx frame into memory",  };
static const char *bus_error[8] = {
  "Parity Error", "Master Abort", "Target abort", "Unknown 3",
  "Unknown 4", "Unknown 5", "Unknown 6", "Unknown 7"};
const char *intr_names[16] ={
	"Tx done", "Tx complete", "Tx out of buffers", "Transmit Jabber",
	"Link passed", "Tx FIFO Underflow", "Rx Done", "Receiver out of buffers",
	"Receiver stopped", "Receiver jabber", "Link changed", "Timer expired",
	"Link failed", "PCI bus error", "Early Rx", "Abnormal summary",
};

#define EEPROM_SIZE 256			/* Size may be 256x16 for CardBus. */

static int has_mii = 0;
static int current_part_idx = 0; /* Global, for mdio_{read,write,sync}() */
static int default_media_offset = -1;

/* Values read from the EEPROM, and the new image to write. */
static unsigned short eeprom_contents[EEPROM_SIZE];
static unsigned short new_ee_contents[EEPROM_SIZE];

/* Support for Flash operations. */
static int tulip_flash_in(long ioaddr, int offset) {
	outl(offset, ioaddr + CSR10);
	outl(0x5000, ioaddr + CSR9);
	return inl(ioaddr + CSR9) & 0xff;
}
#ifdef LIBFLASH
static void tulip_flash_out(long ioaddr, int offset, int val) {
	outl(offset, ioaddr + CSR10);
	outl(0x3000 | val, ioaddr + CSR9);
}
#endif

int tulip_diag(int vendor_id, int device_id, long ioaddr, int part_idx)
{
	int flags = pcidev_tbl[part_idx].flags;			/* Capabilities. */
	int if_active = 0;
	int i;

	/* It's mostly safe to use the Tulip EEPROM and MDIO register during
	   operation.  But warn the user, and make then pass '-f'. */
	if (opt_a  &&  !opt_f  && (inl(ioaddr + CSR6) & 0x2002) != 0) {
		printf(" * A potential Tulip chip has been found, but it appears to "
			   "be active.\n * Either shutdown the network, or use the"
			   " '-f' flag to see all values.\n");
		if_active = 1;
	}

	/* We always have registers up to CSR15.
	   We may always safely read up to CSR7.
	   We must be stopped or have '-f' to show CSR8-CSR15, since we might
	   clear the missed packet count and other status.
	   Chips with registers above CSR15 usually space them on 4 byte
	   boundaries instead of 8 byte boundaries.
	 */
	if (show_regs) {
		printf("%s chip registers at %#lx:\n 0x00:",
			   pcidev_tbl[part_idx].part_name, ioaddr);
		for (i = 0; i < 64; i += 8)
			printf(" %8.8x", inl(ioaddr + i));
		printf("\n");
	}
	if (show_regs && (!if_active || opt_f)) {
		int num_regs = pcidev_tbl[part_idx].io_size;
		printf(" 0x40:");
		for (; i < 128; i += 8)
			printf(" %8.8x", inl(ioaddr + i));
		if (i < num_regs) {
			/* Extended registers are _not_ quadword aligned. */
			printf("\n Extended registers:");
			for (; i < num_regs; i += 4) {
				if (i % 32 == 0) printf("\n %2.2x:", i);
				printf(" %8.8x", inl(ioaddr + i));
			}
		}
		printf("\n");
	}
	if (!opt_f  &&  inl(ioaddr + CSR5) == 0xffffffff) {
		printf(" * A recognized chip has been found, but it does not appear"
			   " to exist in\n * I/O space.  Use the"
			   " '-f' flag to see the register values anyway.\n");
		return 1;
	} else {
		int csr5 = inl(ioaddr + CSR5);
		int csr6 = inl(ioaddr + CSR6);

		if (flags & (DC21040_EEPROM | DC21041_EEPROM))
			printf(" Port selection is %s-duplex.\n",
				   csr6 & 0x0200 ? "full" : "half");
		else if (flags & COMET_MACADDR)	{		/* ADMtek chips. */
			printf(" Comet duplex is reported in the MII status registers.\n");
		} else if (flags & PNIC2_MACADDR)	{		/* The PNIC-II chip. */
			int csr14 = inl(ioaddr + CSR14);
			printf(" Port selection is %s, %s-duplex.\n",
				   csr14 & 0x80 ? "N-Way autonegotiation" :
				   csr6 & 0x00040000 ?  "100baseTx" : "10mpbs-serial",
				   csr6 & 0x0200 ? "full" : "half");
		} else
			printf(" Port selection is %s%s, %s-duplex.\n",
				   ! (csr6 & 0x00040000) ? "10mpbs-serial" :
				   (csr6 & 0x00800000 ? "100mbps-SYM/PCS" : "MII"),
				   (csr6 & 0x01800000)==0x01800000 ? " 100baseTx scrambler":"",
				   csr6 & 0x0200 ? "full" : "half");
		printf(" Transmit %s, Receive %s, %s-duplex.\n",
			   csr6 & 0x2000 ? "started" : "stopped",
			   csr6 & 0x0002 ? "started" : "stopped",
			   csr6 & 0x0200 ? "full" : "half");
		printf("  The Rx process state is '%s'.\n",
			   rx_state[(csr5 >> 17) & 7]);
		printf("  The Tx process state is '%s'.\n",
			   tx_state[(csr5 >> 20) & 7]);
		if (csr5 & 0x2000)
			printf("  PCI bus error!: %s.\n",
				   bus_error[(csr5 >> 23) & 7]);
		if (csr6 & 0x00200000)
			printf("  The transmit unit is set to store-and-forward.\n");
		else {
			const short tx_threshold[2][4] = {{ 72, 96, 128, 160 },
											  {128,256, 512, 1024}};
			printf("  The transmit threshold is %d.\n",
				tx_threshold[(csr6&0x00440000) == 0x00040000][(csr6>>14) & 3]);
		}
		if (csr5 & 0x18000) {
			printf(" Interrupt sources are pending!  CSR5 is %8.8x.\n", csr5);
			for (i = 0; i < 15; i++)
				if (csr5 & (1<<i))
					printf("   %s indication.\n", intr_names[i]);
		}
	}

	if (flags & HAS_NWAY)
		printf("  The NWay status register is %8.8x.\n", inl(ioaddr + CSR12));

	if (flags & COMET_MACADDR) {
		printf("  Comet MAC address registers %8.8x %8.8x\n"
			   "  Comet multicast filter %8.8x%8.8x.\n",
			   inl(ioaddr + 0xA4), inl(ioaddr + 0xA8),
			   inl(ioaddr + 0xAC), inl(ioaddr + 0xB0));
	} else if (flags & PNIC2_MACADDR) {
		/* Grrr, damn Lite-On cannot use a consistent byte order. */
		printf(" The current PNIC-II MAC address is "
			   "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x (%8.8x %8.8x).\n",
			   inb(ioaddr + 0xB8), inb(ioaddr + 0xB9), inb(ioaddr + 0xB2),
			   inb(ioaddr + 0xB3), inb(ioaddr + 0xB0), inb(ioaddr + 0xB1),
			   inl(ioaddr + 0xB8), inl(ioaddr + 0xB0));
		printf(" The current PNIC-II WOL address is "
			   "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x.\n",
			   inb(ioaddr + 0xBA), inb(ioaddr + 0xBB), inb(ioaddr + 0xC2),
			   inb(ioaddr + 0xC3), inb(ioaddr + 0xC0), inb(ioaddr + 0xC1));
	}

	/* Below this point might be unsafe while the interface is active. */
	if (if_active && ! opt_f)
		return 1;

	if (flags & ASIX_MACADDR) {
		printf(" The MAC/filter registers are ");
		for (i = 0; i < 4; i++) {
			outl(i, ioaddr + CSR13);
			printf(" %8.8x", inl(ioaddr + CSR14));
		}
		printf(".\n");
	}

	if (opt_GPIO) {
		printf("Setting the GPIO register %8.8x.\n", opt_GPIO);
		outl(opt_GPIO, ioaddr + CSR15);
	}
	tulip_eeprom(ioaddr, flags);

	/* Show up to four (not just the on-board) PHYs. */
	if ((has_mii && verbose) || show_mii) {
		int phys[4], phy, phy_idx = 0;
		current_part_idx = part_idx;		/* Hack, set a global */
		mdio_sync(ioaddr);
		for (phy = 1; phy <= 32 && phy_idx < 4; phy++) {
			int mii_status = mdio_read(ioaddr, phy & 0x1f, 1);
			if (mii_status != 0xffff  &&
				mii_status != 0x0000) {
				phys[phy_idx++] = phy & 0x1f;
				printf(" MII PHY found at address %d, status 0x%4.4x.\n",
					   phy & 0x1f, mii_status);
			}
		}
		if (phy_idx) {
			if (nway_advertise > 0) {
				printf(" Setting the media capability advertisement register"
					   " of PHY #%d to 0x%4.4x.\n",
					   phys[0], nway_advertise | 1);
				mdio_write(ioaddr, phys[0], 4, nway_advertise | 1);
			}
			if (opt_restart) {
				printf("Restarting negotiation...\n");
				mdio_write(ioaddr, phys[0], 0, 0x0000);
				mdio_write(ioaddr, phys[0], 0, 0x1200);
			}
			/* Force 100baseTx-HD by mdio_write(ioaddr,phys[0], 0, 0x2000); */
			if (fixed_speed >= 0) {
				int reg0_val = 0;
				reg0_val |= (fixed_speed & 0x0180) ? 0x2000 : 0;
				reg0_val |= (fixed_speed & 0x0140) ? 0x0100 : 0;
				printf("Setting the speed to \"fixed\", %4.4x.\n", reg0_val);
				mdio_write(ioaddr, phys[0], 0, reg0_val);
			}
		}
		if (phy_idx == 0)
			printf("   No MII transceivers found!\n");
#ifdef LIBMII
		else {
			if (show_mii > 1)
				show_mii_details(ioaddr, phys[0]);
			if (opt_watch || show_mii > 2)
				monitor_mii(ioaddr, phys[0]);
		}
#else
		else for (phy = 0; phy < phy_idx; phy++) {
			int mii_reg;
			printf(" MII PHY #%d transceiver registers:", phys[phy]);
			for (mii_reg = 0; mii_reg < 32; mii_reg++)
				printf("%s %4.4x", (mii_reg % 8) == 0 ? "\n  " : "",
					   mdio_read(ioaddr, phys[phy], mii_reg));
			printf(".\n");
		}
#endif
	}
	if (flags & HAS_NWAY)
		setup_nway_xcvr(ioaddr);

#ifdef LIBFLASH
	flash_in_hook = tulip_flash_in;
	flash_out_hook = tulip_flash_out;
	if (opt_flash_show)
		flash_show(ioaddr, 0);
	if (opt_flash_dumpfile)
		if (flash_dump(ioaddr, 0, opt_flash_dumpfile) < 0) {
			fprintf(stderr, "Failed to save the old Flash BootROM image into"
					" file '%s'.\n", opt_flash_dumpfile);
			return 3;
		}
	if (opt_flash_loadfile) {
		outl(0x3000, ioaddr + CSR9);
		if (flash_program(ioaddr, 0, opt_flash_loadfile) < 0) {
			fprintf(stderr, "Failed to load the new Flash BootROM image from"
					" file '%s'.\n", opt_flash_loadfile);
			return 4;
		}
	}
#else
	if (opt_flash_loadfile  || opt_flash_dumpfile  ||  opt_flash_show)
		printf("Flash operations not configured into this program.\n");
	if (opt_flash_show) {
		printf("The first few boot ROM bytes are:");
		for (i = 0; i < 8; i++)
			printf(" %2.2x", tulip_flash_in(ioaddr, i));
		printf(".\n");
	}
#endif

	return 0;
}

#include <ctype.h>
static void tulip_eeprom(long ioaddr, int flags)
{
	int eeprom_words = 64;
	int i;

	/* Read the EEPROM. */
	memset(eeprom_contents, 0, sizeof(eeprom_contents));
	if (flags & DC21040_EEPROM) {
		outl(0, ioaddr + CSR9);		/* Reset the pointer with a dummy write. */
		for (i = 0; i < 128; i++) {
			int value, boguscnt = 100000, sum = 0;
			do
				value = inl(ioaddr + CSR9);
			while (value < 0  && --boguscnt > 0);
			((unsigned char *)eeprom_contents)[i] = value;
			if (i < 6)
				sum += value & 0xff;
		}
	} else if (flags & PNIC_EEPROM) {
		for (i = 0; i < eeprom_words; i++) {
			int value, boguscnt = 100000;
			unsigned short sum;
			outl(0x600 | i, ioaddr + 0x98);
			do
				value = inl(ioaddr + CSR9);
			while (value < 0  && --boguscnt > 0);
			((unsigned short *)eeprom_contents)[i] = value;
			sum += value & 0xffff;
		}
	} else {
		u16 *eew = (u16 *)eeprom_contents;
		u16 andsum = 0xffff;
		int ee_addr_size = read_eeprom(ioaddr, 0xff, 8) & 0x40000 ? 8 : 6;
		eeprom_words = ee_addr_size == 8 ? 256 : 64;

		if (show_eeprom)
			printf("EEPROM %d words, %d address bits.\n",
				   eeprom_words, ee_addr_size);

		for (i = 0; i < eeprom_words; i++)
			andsum &= (eew[i] = read_eeprom(ioaddr, i, ee_addr_size));
		if (andsum == 0xffff)
			printf("WARNING: The EEPROM is missing or erased!\n");
	}

	/* The user will usually want to see the interpreted EEPROM contents. */
	if (show_eeprom || verbose > 1) {
		if (flags & DC21040_EEPROM)
			;
		else if (flags & PNIC_MTABLE)
			liteon_eeprom((unsigned char *)eeprom_contents, flags);
		else if (flags & COMET_MACADDR)			/* All ADMtek chips. */
			admtek_eeprom((unsigned char *)eeprom_contents, flags);
		else if (flags & IS_CONEXANT)
			conexant_eeprom((unsigned char *)eeprom_contents, flags);
		else if (flags & IS_CONEXANT)
			davicom_eeprom((unsigned char *)eeprom_contents, flags);
		else
			parse_eeprom((unsigned char *)eeprom_contents, flags);

		if (show_eeprom > 1) {
			int block_crc = (calculate_checksum1(eeprom_contents, 8)>>8) & 0xff;
			int full_crc =
				(ether_crc_le((void*)eeprom_contents, 126) ^ 0xffff) & 0xffff;
			printf("EEPROM contents (%d words):", eeprom_words);
			for (i = 0; i < eeprom_words; i += 8) {
				int j;
				printf("\n0x%2.2x: ", i);
				for (j = 0; j < 8; j++)
					printf(" %4.4x", eeprom_contents[i + j]);
				if (show_eeprom > 2) {
					printf("  ");
					for (j = 0; j < 8; j++) {
						int ew = eeprom_contents[i + j];
						printf("%c%c",
							   isalnum(ew & 0xff) ? ew & 0xff : '_',
							   isalnum(ew >>   8) ? ew >> 8   : '_' );
					}
				}
			}
			printf("\n ID block CRC %#2.2x (vs. %#2.2x).\n"
				   "  Full contents CRC 0x%4.4x (read as 0x%4.4x).\n",
				   block_crc, eeprom_contents[8] & 0xff,
				   full_crc, eeprom_contents[63]);
		}
	}
	/* Check for a bogus Intel CardBus card. */
	if (eeprom_contents[0] == 0x8086  &&  eeprom_contents[1] == 0x0001  &&
		eeprom_contents[2] == 0x0087)
		check_for_intel_cb(ioaddr, eeprom_contents);
	if (new_default_media >= 0) {
		if (default_media_offset > 0)
			do_update(ioaddr, eeprom_contents, default_media_offset/2,
					  "Default Media", mediamap[new_default_media].value);
	}
}

/* Reading a serial EEPROM is a "bit" grungy, but we work our way through:->.*/
/* This code is a "nasty" timing loop, but PC compatible machines are
   *supposed* to delay an ISA-compatible period for the SLOW_DOWN_IO macro.  */
#define eeprom_delay()	inl(ee_addr)

/*  EEPROM_Ctrl bits. */
#define EE_SHIFT_CLK	0x02	/* EEPROM shift clock. */
#define EE_CS			0x01	/* EEPROM chip select. */
#define EE_DATA_WRITE	0x04	/* EEPROM chip data in. */
#define EE_WRITE_0		0x4801
#define EE_WRITE_1		0x4805
#define EE_DATA_READ	0x08	/* EEPROM chip data out. */
#define EE_ENB			(0x4800 | EE_CS)

/* The EEPROM commands include the alway-set leading bit. */
#define EE_WRITE_CMD	(5)
#define EE_READ_CMD		(6)
#define EE_ERASE_CMD	(7)

/* Note: this routine returns extra data bits for size detection. */
static int read_eeprom(long ioaddr, int location, int addr_len)
{
	int i;
	unsigned retval = 0;
	long ee_addr = ioaddr + CSR9;
	int read_cmd = location | (EE_READ_CMD << addr_len);

	outl(EE_ENB & ~EE_CS, ee_addr);
	outl(EE_ENB, ee_addr);

	if (debug > 2)
		printf(" EEPROM read at %d ", location);

	/* Shift the read command bits out. */
	for (i = 4 + addr_len; i >= 0; i--) {
		short dataval = (read_cmd & (1 << i)) ? EE_DATA_WRITE : 0;
		outl(EE_ENB | dataval, ee_addr);
		eeprom_delay();
		outl(EE_ENB | dataval | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
		if (debug > 2)
			printf("%X", inl(ee_addr) & 15);
		retval = (retval << 1) | ((inl(ee_addr) & EE_DATA_READ) ? 1 : 0);
	}
	outl(EE_ENB, ee_addr);
	if (debug > 2)
		printf(" :%X:", inl(ee_addr) & 15);

	for (i = 16; i > 0; i--) {
		outl(EE_ENB | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
		if (debug > 2)
			printf("%X", inl(ee_addr) & 15);
		retval = (retval << 1) | ((inl(ee_addr) & EE_DATA_READ) ? 1 : 0);
		outl(EE_ENB, ee_addr);
		eeprom_delay();
	}

	/* Terminate the EEPROM access. */
	outl(EE_ENB & ~EE_CS, ee_addr);
	if (debug > 2)
		printf(" EEPROM value at %d is %5.5x.\n", location, retval);
	return retval;
}

/* This executes a generic EEPROM command, typically a write or write enable.
   It returns the data output from the EEPROM, and thus may also be used for
   reads. */
static int do_eeprom_cmd(long ioaddr, int cmd, int cmd_len)
{
	unsigned retval = 0;
	long ee_addr = ioaddr + CSR9;

	if (debug > 1)
		printf(" EEPROM op 0x%x: ", cmd);

	outl(EE_ENB | EE_SHIFT_CLK, ee_addr);

	/* Shift the command bits out. */
	do {
		short dataval = (cmd & (1 << cmd_len)) ? EE_WRITE_1 : EE_WRITE_0;
		outl(dataval, ee_addr);
		eeprom_delay();
		if (debug > 2)
			printf("%X", inl(ee_addr) & 15);
		outl(dataval | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
		retval = (retval << 1) | ((inl(ee_addr) & EE_DATA_READ) ? 1 : 0);
	} while (--cmd_len >= 0);
	outl(EE_ENB, ee_addr);

	/* Terminate the EEPROM access. */
	outl(EE_ENB & ~EE_CS, ee_addr);
	if (debug > 1)
		printf(" EEPROM result is 0x%5.5x.\n", retval);
	return retval;
}

/* Read and write the MII registers using software-generated serial
   MDIO protocol.  It is just different enough from the EEPROM protocol
   to not share code.  The maxium data clock rate is 2.5 Mhz. */
#define mdio_delay()  inl(mdio_addr)	/* Extra bus turn-around as a delay. */

#define MDIO_SHIFT_CLK	0x10000
#define MDIO_DATA_WRITE0 0x00000
#define MDIO_DATA_WRITE1 0x20000
#define MDIO_ENB		0x00000		/* Ignore the 0x02000 databook setting. */
#define MDIO_ENB_IN		0x40000
#define MDIO_DATA_READ	0x80000

/* Syncronize the MII management interface by shifting 32 one bits out. */
static void mdio_sync(long ioaddr)
{
	long mdio_addr = ioaddr + CSR9;
	int i;

	if (pcidev_tbl[current_part_idx].flags & (PNIC_MII|COMET_MII))
		return;
	for (i = 32; i >= 0; i--) {
		outl(MDIO_ENB | MDIO_DATA_WRITE1, mdio_addr);
		mdio_delay();
		outl(MDIO_ENB | MDIO_DATA_WRITE1 | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay();
	}
	return;
}

int mdio_read(long ioaddr, int phy_id, int location)
{
	int i;
	int read_cmd = (0xf6 << 10) | (phy_id << 5) | location;
	int retval = 0, saved_0xfc = 0;
	long mdio_addr = ioaddr + CSR9;

	if (verbose > 2)		/* Debug: 5 */
		printf(" mdio_read(%#lx, %d, %d)..", ioaddr, phy_id, location);
	if (pcidev_tbl[current_part_idx].flags & PNIC_MII) {
		int i = 1000;
		outl(0x60020000 + (phy_id<<23) + (location<<18), ioaddr + 0xA0);
		inl(ioaddr + 0xA0);
		inl(ioaddr + 0xA0);
		while (--i > 0)
			if ( ! ((retval = inl(ioaddr + 0xA0)) & 0x80000000))
				break;
		if (debug)
			printf("Register at %#lx is %#x (%#x).\n", ioaddr,
				   inl(ioaddr + 0xA0), retval & 0xffff);
		return retval & 0xffff;
	}
	if (pcidev_tbl[current_part_idx].flags & COMET_MII) {
		if (phy_id == 1) {
			if (location < 7)
				return inl(ioaddr + 0xB4 + (location<<2));
			else if (location == 17)
				return inl(ioaddr + 0xD0);
			else if (location >= 29 && location <= 31)
				return inl(ioaddr + 0xD4 + ((location-29)<<2));
		}
		return 0xffff;
	}
	if (pcidev_tbl[current_part_idx].flags & COMET_HPNA) {
		saved_0xfc = inl(ioaddr + 0xfc);
		if (phy_id == 1) outl(0x00, ioaddr + 0xfc);
		else if (phy_id == 2) outl(0x24, ioaddr + 0xfc);
	}
			
	/* Establish sync by sending at least 32 logic ones. */
	for (i = 32; i >= 0; i--) {
		outl(MDIO_ENB | MDIO_DATA_WRITE1, mdio_addr);
		mdio_delay();
		outl(MDIO_ENB | MDIO_DATA_WRITE1 | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay();
	}
	/* Shift the read command bits out. */
	for (i = 17; i >= 0; i--) {
		int dataval = (read_cmd & (1 << i)) ? MDIO_DATA_WRITE1 : 0;
		if (verbose > 3)		/* Debug: 5 */
			printf("%d", (read_cmd & (1 << i)) ? 1 : 0);

		outl(MDIO_ENB | dataval, mdio_addr);
		mdio_delay();
		outl(MDIO_ENB | dataval | MDIO_SHIFT_CLK, mdio_addr);
		if (verbose > 3) printf(" %x", (inl(mdio_addr) >> 16) & 0x0f);
		mdio_delay();
	}
	if (verbose > 3) printf("-> %x", (inl(mdio_addr) >> 16) & 0x0f);

	/* Read the two transition, 16 data, and wire-idle bits. */
	for (i = 19; i > 0; i--) {
		outl(MDIO_ENB_IN, mdio_addr);
		mdio_delay();
		retval = (retval << 1) | ((inl(mdio_addr) & MDIO_DATA_READ) ? 1 : 0);
		if (verbose > 3) printf(" %x", (inl(mdio_addr) >> 16) & 0x0f);
		outl(MDIO_ENB_IN | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay();
	}
	if (verbose > 3) printf(" == %4.4x.\n", retval);
	if (pcidev_tbl[current_part_idx].flags & COMET_HPNA)
		outl(saved_0xfc, ioaddr + 0xfc);
	return (retval>>1) & 0xffff;
}

void mdio_write(long ioaddr, int phy_id, int location, int value)
{
	int i;
	int cmd = (0x5002 << 16) | (phy_id << 23) | (location<<18) | value;
	long mdio_addr = ioaddr + CSR9;

	if (pcidev_tbl[current_part_idx].flags & COMET_MII) {
		if (phy_id == 1) {
			if (location < 7)
				outl(value, ioaddr + 0xB4 + (location<<2));
			else if (location == 17)
				outl(value, ioaddr + 0xD0);
			else if (location >= 29 && location <= 31)
				outl(value, ioaddr + 0xD4 + ((location-29)<<2));
		}
		return;
	}
	/* Establish sync by sending 32 logic ones. */
	for (i = 32; i >= 0; i--) {
		outl(MDIO_ENB | MDIO_DATA_WRITE1, mdio_addr);
		mdio_delay();
		outl(MDIO_ENB | MDIO_DATA_WRITE1 | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay();
	}
	/* Shift the command bits out. */
	for (i = 31; i >= 0; i--) {
		int dataval = (cmd & (1 << i)) ? MDIO_DATA_WRITE1 : 0;
		outl(MDIO_ENB | dataval, mdio_addr);
		mdio_delay();
		outl(MDIO_ENB | dataval | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay();
	}
	/* Clear out extra bits. */
	for (i = 2; i > 0; i--) {
		outl(MDIO_ENB_IN, mdio_addr);
		mdio_delay();
		outl(MDIO_ENB_IN | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay();
	}
	return;
}

#include <sys/time.h>

const char *nway_state[8] = {
	"Autonegotiation disabled", "Transmit disabled", "Ability detect",
	"Acknowledge detect", "Complete acknowledge", "Negotiation complete",
	"Link check", "Invalid state"
};

static void setup_nway_xcvr(long ioaddr)
{
	int csr12 = inl(ioaddr + CSR12);
	int i;
	printf("  Internal autonegotiation state is '%s'.\n",
		   nway_state[(csr12 >> 12) & 7]);
	if (opt_restart) {		/* Restart NWay. */
		int csr6 = inl(ioaddr + CSR6) & 0xFDFF;
		if (nway_advertise > 0) {
			int csr_setting = ((nway_advertise & 0x0380) << 9) |
				((nway_advertise & 0x0020) << 1);
			if (verbose)
				printf("Setting 21142 N-Way advertisement to %4.4x "
					   "(%x).\n", nway_advertise, csr_setting );
			outl(0x000FFBF | csr_setting, ioaddr + CSR14);
			outl((nway_advertise & 0x0040 ? 0x82420200 : 0x82420000) | csr6,
				 ioaddr + CSR6);
		} else {
			outl(0x0003FFFF, ioaddr + CSR14);
			outl(0x82420200 | csr6, ioaddr + CSR6);
		}
#if 0
		printf("  Writing the CSR15 direction bits.\n");
		outl(0x08af0000, ioaddr + CSR15);
		outl(0x00050000, ioaddr + CSR15);
#endif
		if (opt_reset) {
			printf("  Resetting the 21143 SIA registers.\n");
			outl(0x0000, ioaddr + CSR13);
			outl(0x0001, ioaddr + CSR13);
		}
		outl(0x1000, ioaddr + CSR12); /* Start NWay. */
		csr12 = inl(ioaddr + CSR12);
		printf("  Internal autonegotiation state is now '%s' CSR12 %x.\n"
			   "    CSR13 %x CSR14 %x CSR15 %x.\n",
			   nway_state[(csr12 >> 12) & 7],
			   csr12, inl(ioaddr + CSR13),
			   inl(ioaddr + CSR14), inl(ioaddr + CSR15));
		for (i = 0; i < 400; i++) {
			struct timeval sleepval;
			sleepval.tv_sec = 0;
			sleepval.tv_usec = 10000;
			select(0, 0, 0, 0, &sleepval);			/* Or just sleep(1); */
			if (csr12 != inl(ioaddr + CSR12)) {
				csr12 = inl(ioaddr + CSR12);
				printf("  Internal autonegotiation state is now '%s', "
					   "CSR12 %x.\n"
					   "    CSR5 %x CSR13 %x CSR14 %x CSR15 %x.\n",
					   nway_state[(csr12 >> 12) & 7],
					   csr12, inl(ioaddr + CSR5), inl(ioaddr + CSR13),
					   inl(ioaddr + CSR14), inl(ioaddr + CSR15));
			}
		}
		printf(" Final autonegotiation state is '%s', CSR12 %x.\n"
			   "    CSR5 %x CSR13 %x CSR14 %x CSR15 %x.\n",
			   nway_state[(inl(ioaddr + CSR12) >> 12) & 7],
			   inl(ioaddr + CSR12), inl(ioaddr + CSR5), inl(ioaddr + CSR13),
			   inl(ioaddr + CSR14), inl(ioaddr + CSR15));
		/* We must explicitly switch to 100mbps mode. */
		if (((nway_advertise > 0 ? nway_advertise : 0x01e1) &
			 inl(ioaddr + CSR12) >> 16) & 0x0180)
			outl(0x83860200, ioaddr + CSR6);
	}
}

/* Calculate the EEPROM checksums. */

#define CRC1_POLYNOMIAL 0x07		/* x^8 + x^2 + x^1 + 1 */
static unsigned int
calculate_checksum1(unsigned short *eeprom, int len)
{
	u16 crc = 0xffff;
	int i, bit;

	for (i = 0; i <= len; i++)				/* Note: loc. 18 is the sum. */
		for (bit = 15; bit >= 0; bit--) {
			/* Note: bits ordered as read from EEPROM */
			crc <<= 1;
			if (((eeprom[i]>>bit) ^ (crc >> 8)) & 1)
				crc ^= CRC1_POLYNOMIAL;
		}
	return crc;
}

static unsigned const ethernet_polynomial_le = 0xedb88320U;
static unsigned int ether_crc_le(void *ptr, int length)
{
	unsigned char *data = ptr;
	unsigned int crc = 0xffffffff;	/* Initial value. */
	while(--length >= 0) {
		unsigned char current_octet = *data++;
		int bit;
		for (bit = 8; --bit >= 0; current_octet >>= 1) {
			if ((crc ^ current_octet) & 1) {
				crc >>= 1;
				crc ^= ethernet_polynomial_le;
			} else
				crc >>= 1;
		}
	}
	return crc;
}

int do_update(long ioaddr, unsigned short *ee_values,
			  int index, char *field_name, int new_value)
{
	if (ee_values[index] == new_value)
		return 0;
	if (do_write_eeprom) {
		int ee_addr_size = read_eeprom(ioaddr, 0xff, 8) & 0x40000 ? 8 : 6;
		int i;
		u16 newval;
		printf("Writing new %s entry 0x%4.4x to offset %d.\n",
			   field_name, new_value, index);
		/* Enable programming modes. */
		do_eeprom_cmd(ioaddr, (0x4f << (ee_addr_size-4)), 3+ee_addr_size);
		/* Do the actual write. */
		do_eeprom_cmd(ioaddr,
					  (((EE_WRITE_CMD<<ee_addr_size)|index) << 16) | new_value,
					  3 + ee_addr_size + 16);
		/* Poll for write finished. */
		outl(EE_ENB, ioaddr + CSR9);
		for (i = 0; i < 10000; i++)			/* Typical 2000 ticks */
			if (inl(ioaddr + CSR9) & EE_DATA_READ)
				break;
		if (debug)
			printf(" Write finished after %d ticks.\n", i);
		/* Disable programming. */
		do_eeprom_cmd(ioaddr, (0x40 << (ee_addr_size-4)), 3 + ee_addr_size);
		/* And read the result. */
		newval = do_eeprom_cmd(ioaddr,
							   (((EE_READ_CMD<<ee_addr_size)|index) << 16)
							   | 0xffff, 3 + ee_addr_size + 16);
		printf("  New %s value at offset %d is %4.4x.\n",
			   field_name, index, newval);
	} else
		printf(" Would write new %s entry 0x%4.4x to offset %d, the "
			   "current value is 0x%4.4x.\n",
			   field_name, new_value, index, ee_values[index]);
	ee_values[index] = new_value;
	return 1;
}


/* Parse and emit the information from the EEPROM table. */

static const char * const medianame[32] = {
  "10baseT", "10base2", "AUI", "100baseTx",
  "10baseT-Full Duplex", "100baseTx Full Duplex", "100baseT4", "100baseFx",
  "100baseFx-Full Duplex", "MII 10baseT", "MII 10baseT-Full Duplex", "MII",
  "", "MII 100baseTx", "MII 100baseTx-Full Duplex", "MII 100baseT4",
  "MII 100baseFx-HDX", "MII 100baseFx-FDX", "Home-PNA 1Mbps", "",
};

struct mediainfo {
	struct mediainfo *next;
	int info_type;
	struct non_mii { char media, csr12val, bitnum, flags;}  non_mii;
	unsigned char *mii;
};

static void parse_eeprom(unsigned char *ee_data, int flags)
{
	unsigned char csr12;
	unsigned char *p;
	int count, sum, media_idx, i;
	int default_media;

	for (i = 0, sum = 0xff; i < 6; i++)
		sum &= ee_data[i];
	if (sum == 0xff) {
		printf(" This interface is missing the EEPROM.\n  This is likely the "
			   "non-primary interface on a multiport board.\n");
		return;
	}
	/* Detect an old-style (SA only) EEPROM layout. */
	if (memcmp(ee_data, ee_data + 16, 8) == 0) {
	  /* Should actually do a fix-up based on the vendor half of the station
		 address prefix here.  Or least use that information to report which
		 transceiver will work. */
		printf("  * An old-style EEPROM layout was found.
  * The old-style layout does not contain transceiver control information.
  * This board may not work, or may work only with a subset of transceiver
  * options or data rates.\n");
	  return;
	} else if (ee_data[27] == 0) {
		printf(" A simplifed EEPROM data table was found.\n"
			   " The EEPROM does not contain transceiver control information.\n");
		return;
	}

	printf("PCI Subsystem IDs, vendor %2.2x%2.2x, device %2.2x%2.2x.\n"
		   "CardBus Information Structure at offset %2.2x%2.2x%2.2x%2.2x.\n",
		   ee_data[1], ee_data[0], ee_data[3], ee_data[2],
		   ee_data[7], ee_data[6], ee_data[5], ee_data[4]);

	printf("Ethernet MAC Station Address "
		   "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X.\n",
		   ee_data[20], ee_data[21], ee_data[22],
		   ee_data[23], ee_data[24], ee_data[25]);

	if (ee_data[19] != 1)
		printf("This table is for a multiport card with %d controllers.\n",
			   ee_data[19]);

	printf("EEPROM transceiver/media description table.\n");
	if (ee_data[27] == 0)
		return;
	default_media_offset = ee_data[27];
	p = (void *)ee_data + ee_data[27];
	default_media = get_unaligned((u16 *)p);
	p += 2;
	printf("Leaf node at offset %d, default media type %4.4x (%s).\n",
		   ee_data[27], default_media, default_media & 0x0800 ? "Autosense"
		   : medianame[default_media & 31]);
	if (flags & CSR12_IS_GPIO) {
		csr12 = *p++;
		printf(" CSR12 direction setting bits 0x%2.2x.\n", csr12);
	}
	count = *p++;
	printf(" %d transceiver description blocks:\n", count);

	for (media_idx = 0; media_idx < count; media_idx++) {
		if (flags & 0x01) {
			printf("  21041 media index %2.2x (%s).\n",
				   p[0] & 0x3f, medianame[p[0] & 31]);
			if (p[0] & 0x40) {
				printf("   CSR Register override settings for this media:"
					   " %2.2x%2.2x %2.2x%2.2x %2.2x%2.2x.\n",
					   p[2], p[1], p[4], p[3], p[6], p[5]);
				p += 7;
			} else
				p += 1;
		} else if (p[0] & 0x80) { /* Extended block */
			int blk_type = p[1];
			if (show_eeprom)
				printf("  Media %s, block type %d, length %d.\n",
					   p[1] & 1  ?  "MII"  :  medianame[p[2] & 31],
					   blk_type, p[0] & 0x7f);
			switch (blk_type) {
			case 0:
				printf("   AUI or SYM transceiver for %s (media type %d).\n"
					   "    CSR12 control port setting %#2.2x,"
					   " command 0x%2.2x 0x%2.2x.\n",
					   medianame[p[2] & 31], p[2], p[3], p[5], p[4]);
				if (p[5] & 0x80) {
					printf("    No media-active status available.\n");
					break;
				}
				printf("    Media detection by looking for a %d on bit %d of"
					   " the CSR12 control port.\n",
					   (p[4] & 0x80) ? 0 : 1, (p[4] >> 1) & 7);
				break;
			case 1:				/* 21140 MII PHY*/
			case 3:	{			/* 21142 MII PHY */
				int init_length = p[3];
				u16 *misc_info;

				printf("   MII interface PHY %d (media type 11).\n", p[2]);
				if (blk_type == 3) {	/* 21142 */
					u16 *init_sequence = (u16*)(p+4);
					u16 *reset_sequence = &((u16*)(p+5))[init_length];
					int reset_length = p[4 + init_length*2];
					misc_info = reset_sequence + reset_length;
					printf("   21143 MII initialization sequence is %d "
						   "words:", init_length);
					for (i = 0; i < init_length; i++)
						printf(" %4.4x", get_unaligned(init_sequence + i));
					printf(".\n   21143 MII reset sequence is %d words:",
						   reset_length);
					for (i = 0; i < reset_length; i++)
						printf(" %4.4x", get_unaligned(reset_sequence + i));
					printf(".\n");
				} else {
					u8 *init_sequence = p + 4;
					u8 *reset_sequence = p + 5 + init_length;
					int reset_length = p[4 + init_length];
					misc_info = (u16*)(reset_sequence + reset_length);
					if (reset_length) {
						printf("    21140 MII Reset sequence is %d words:",
							   reset_length);
						for (i = 0; i < reset_length; i++)
							printf(" %2.2x", reset_sequence[i]);
					} else
						printf("    No MII reset sequence.");
					if (init_length) {
						printf(".\n    21140 MII initialization sequence is "
							   "%d words:", init_length);
						for (i = 0; i < init_length; i++)
							printf(" %2.2x", init_sequence[i]);
						printf(".\n");
					} else
						printf("    No MII initialization sequence.\n");
				}

				printf("    Media capabilities are %4.4x, advertising %4.4x.\n"
					   "    Full-duplex map %4.4x, Threshold map %4.4x.\n",
					   get_unaligned(misc_info + 0),
					   get_unaligned(misc_info + 1) | 1,
					   get_unaligned(misc_info + 2),
					   get_unaligned(misc_info + 3));
				if (blk_type == 3) {	/* 21142 */
					if ((*(u8 *)(misc_info+4)) > 0)
						printf("    MII interrupt on GPIO pin %d.\n",
							   (*(u8 *)(misc_info+3)) - 1);
					else
						printf("    No MII interrupt.\n");
				}
				has_mii++;
				break;
			}
			case 2:				/* 21142 SYM or AUI */
			case 4:
				printf("   %s transceiver for %s (media type %d).\n",
					   blk_type == 2 ? "Serial" : "SYM",
					   medianame[p[2] & 31], p[2]);
				if ( ! show_eeprom)
					break;
				if (p[2] & 0x40)
					printf("    CSR13 %2.2x%2.2x  CSR14 %2.2x%2.2x"
						   "  CSR15 %2.2x%2.2x.\n    GP pin direction "
						   "%2.2x%2.2x  GP pin data %2.2x%2.2x.\n",
						   p[4], p[3], p[6], p[5], p[8], p[7], p[10], p[9],
						   p[12], p[11]);
				else
					printf("    GP pin direction %2.2x%2.2x  "
						   "GP pin data %2.2x%2.2x.\n",
						   p[4], p[3], p[6], p[5]);
				if (blk_type == 4) {
					if (p[8] & 0x80)
						printf("    No media detection indication (command "
							   "%2.2x %2.2x).\n", p[8], p[7]);
					else
						printf("    Media detection by looking for a %d on "
							   "general purpose pin %d.\n",
							   (p[7] & 0x80) ? 0 : 1, (p[7] >> 1) & 7);
				}
				break;
			case 5:
				printf("   Transceiver Reset, sequence length %d:", p[2]);
				for( i = 0; i < p[2]; i++)
					printf(" %2.2x%2.2x", p[i*2 + 4], p[i*2 + 3]);
				printf(".\n");
				break;
			case 6:
				printf("   Disconnect reset, sequence length %d:", p[3]);
				for( i = 0; i < p[3]; i++)
					printf(" %2.2x%2.2x", p[i*2 + 5], p[i*2 + 4]);
				printf(".\n");
				break;
			default:
				printf("   UNKNOW MEDIA DESCRIPTION BLOCK TYPE!\n   ");
				for(i = 1; i <= (p[0] & 0x1f); i++)
					printf(" %2.2x", p[i]);
				printf(".\n");
				break;
			}
			p += (p[0] & 0x3f) + 1;
		} else {				/* "Compact" blocks (aka design screw-up). */
			printf("  21140 Non-MII transceiver for media %d (%s).\n"
				   "   CSR12 control port setting %#2.2x,"
				   " command %#2.2x %#2.2x.\n",
				   p[0], medianame[p[0] & 31], p[1], p[3], p[2]);
			if (p[3] & 0x80) {
				printf("   No media-active status available.\n");
			} else
				printf("   Media detection by looking for a %d on bit %d of"
					   " the CSR12 control port.\n",
					   (p[2] & 0x80) ? 0 : 1, (p[2] >> 1) & 7);
			p += 4;
		}
	}

	if (ee_data[19] >= 4) {		/* Show the Magic Packet block. */
		int b = 128 - 32;		/* Magic Packet block offset */
		int magic_cmd = ee_data[b + 12];
		printf(" The Magic Packet address is "
			   "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X.\n",
			   ee_data[b + 6], ee_data[b + 7], ee_data[b + 8],
			   ee_data[b + 9], ee_data[b + 10], ee_data[b + 11]);
		if (magic_cmd & 2)
			printf(" The Magic Packet password is "
				   "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X.\n",
				   ee_data[b + 0], ee_data[b + 1], ee_data[b + 2],
				   ee_data[b + 3], ee_data[b + 4], ee_data[b + 5]);
	}
}

static void liteon_eeprom(unsigned char *ee_data, int part_idx)
{
	printf("  Ethernet MAC Station Address "
		   "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x.\n",
		   ee_data[20], ee_data[21], ee_data[22],
		   ee_data[23], ee_data[24], ee_data[25]);
	/* Note: This code matches the documentation, but I suspect that it is
	   the documentation that is byte-reversed. */
	printf("  Wake-On-LAN ID bytes "
		   "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x.\n",
		   ee_data[27], ee_data[26], ee_data[29], ee_data[28],
		   ee_data[59], ee_data[58]);
	printf("  PCI Subsystem IDs  Vendor %2.2x%2.2x Device %2.2x%2.2x\n",
		   ee_data[93], ee_data[92], ee_data[90], ee_data[91]);
}

static struct alist { int num; const char *name; }
admtek_media[] = { {0x0000, "10baseT"}, {0x0001, "BNC"}, {0x0002, "AUI"},
				   {0x0003, "100baseTx"}, {0x0004, "100baseT4"},
				   {0x0005, "100baseFx"}, {0x0010, "10baseT-FDX"},
				   {0x0013, "100baseTx-FDX"}, {0x0015, "100baseFx-FDX"},
				   {0x0100, "Autonegotiation"},{0x0200, "Power-on autosense"},
				   {0x0400, "Autosense"}, {0xFFFF, "Default"}, {0, ""}, };
static void admtek_eeprom(unsigned char *ee_data, int part_idx)
{
	unsigned short *eew = (void *)ee_data;
	int i;
	printf("  Ethernet MAC Station Address "
		   "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x.\n",
		   ee_data[8], ee_data[9], ee_data[10],
		   ee_data[11], ee_data[12], ee_data[13]);
	for (i = 0; admtek_media[i].name; i++)
		if (admtek_media[i].num == eew[0x09]) {
			printf("  Default connection type '%s'.\n", admtek_media[i].name);
			break;
		}
	if (admtek_media[i].name == NULL)
		printf("  Unknown default connection type '%#x'.\n", eew[0x09]);
	printf("  PCI IDs Vendor %4.4x Device %4.4x "
		   " Subsystem %4.4x %4.4x\n",
		   eew[0x11], eew[0x10], eew[0x13], eew[0x12]);
	printf("  PCI min_grant %d max_latency %d.\n",
		   ee_data[0x28], ee_data[0x29]);
	printf("  CSR18 power-up setting 0x%4.4x****.\n", eew[0x17]);
}

static void conexant_eeprom(unsigned char *ee_data, int part_idx)
{
	printf("Conexant EEPROM format is undocumented.\n");
}

static void davicom_eeprom(unsigned char *ee_data, int part_idx)
{
	printf("PCI Subsystem IDs, vendor %2.2x%2.2x, device %2.2x%2.2x.\n"
		   "CardBus Information Structure at offset %2.2x%2.2x%2.2x%2.2x.\n",
		   ee_data[1], ee_data[0], ee_data[3], ee_data[2],
		   ee_data[7], ee_data[6], ee_data[5], ee_data[4]);
	printf(" Checksum: calculated %4.4x vs %4.4x from EEPROM.\n",
		   ~ether_crc_le(ee_data, 126) & 0xffff, ee_data[63]);
}

static void check_for_intel_cb(long ioaddr, unsigned short *eeprom_contents)
{
	unsigned const char cis_addr_prefix[4] = {0x22, 0x08, 0x04, 0x06};
	unsigned const short media_ctrl_tbl[] = {
		0x0103, 0x1100, 0x3322, 0x5544, 0x1e00, 0x0000, 0x0800, 0x8604,
		0x0002, 0x08af, 0x00a5, 0x0286, 0xaf04, 0xa508, 0x8800, 0x0304,
		0x08af, 0x00a5, 0x8061, 0x0488, 0xaf05, 0xa508, 0x6100,  };
	unsigned char cis_addr_tuple[10];
	int i;

	printf("You have an Intel CardBus card with an incomplete EEPROM.\n");
	for (i = 0; i < 10; i++)
		cis_addr_tuple[i] = tulip_flash_in(ioaddr, 0xeb + i);
	if (memcmp(cis_addr_tuple, cis_addr_prefix, 4)) {
		printf(" I could not locate the station address.\n");
		return;
	}
	printf("The station address is ");
	for (i = 0; i < 5; i++)
		printf("%2.2x:", cis_addr_tuple[4 + i]);
	printf("%2.2x\n", cis_addr_tuple[4 + i]);
	memcpy(new_ee_contents, eeprom_contents, 18);
	memcpy(new_ee_contents + 9, media_ctrl_tbl, sizeof(media_ctrl_tbl));
	memcpy(new_ee_contents + 10, cis_addr_tuple + 4, 6);
	new_ee_contents[63] = (ether_crc_le(eeprom_contents, 126) ^ 0xffff) & 0xffff;
	printf("New EEPROM contents would be:");
	for (i = 0; i < 64; i++)
		printf("%s %4.4x", (i & 7) == 0 ? "\n ":"", new_ee_contents[i]);
	printf("\n ID CRC %#2.2x (vs. %#2.2x), complete CRC %4.4x.\n",
		   (calculate_checksum1(new_ee_contents, 8) >> 8) & 0xff,
		   new_ee_contents[8] & 0xff, new_ee_contents[63]);
	if (do_write_eeprom) {
		for (i = 0; i < 64; i++)
			do_update(ioaddr, eeprom_contents, i, "Intel media table update",
					  new_ee_contents[i]);
	}
}

/*
 * Local variables:
 *  compile-command: "cc -O -Wall -Wstrict-prototypes -o tulip-diag tulip-diag.c `[ -f libmii.c ] && echo -DLIBMII libmii.c` `[ -f libflash.c ] && echo -DLIBFLASH libflash.c`"
 *  simple-compile-command: "cc -O -o tulip-diag tulip-diag.c"
 *  tab-width: 4
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 */
