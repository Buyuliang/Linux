/*
 *  linux/tools/build.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * This file builds a disk-image from three different files:
 *
 * - bootsect: max 510 bytes of 8086 machine code, loads the rest
 * - setup: max 4 sectors of 8086 machine code, sets up system parm
 * - system: 80386 code for actual system
 *
 * It does some checking that all files are of the correct type, and
 * just writes the result to stdout, removing headers and padding to
 * the right amount. It also writes some system data to stderr.
 */

/*
 * Changes by tytso to allow root device specification
 */

#include <stdio.h>	/* fprintf */
#include <string.h>
#include <stdlib.h>	/* contains exit */
#include <sys/types.h>	/* unistd.h needs this */
#include <sys/stat.h>
#include <linux/fs.h>
#include <unistd.h>	/* contains read/write */
#include <fcntl.h>

/*
 * Changes by falcon<zhangjinw@gmail.com> to define MAJOR and MINOR for they
 * are not defined in current linux header file linux/fs.h,I copy it from
 * include/linux/fs.h directly.
 */

#ifndef MAJOR
	#define MAJOR(a) (((unsigned)(a))>>8)
#endif
#ifndef MINOR
	#define MINOR(a) ((a)&0xff)
#endif

#define MINIX_HEADER 32
#define GCC_HEADER 1024

#define SYS_SIZE 0x3000

/*
 * Changes by falcon<zhangjinw@gmail.com> to let this kernel Image file boot
 * with a root image file on the first hardware device /dev/hd1, hence, you
 * should prepare a root image file, and configure the bochs with
 * the following lines(please set the ... as suitable info):
 * 	...
 *      floppya: 1_44="Image", status=inserted
 *      ata0-master: type=disk, path="/path/to/rootimage.img", mode=flat ...
 *      ...
 */

#define DEFAULT_MAJOR_ROOT 3
#define DEFAULT_MINOR_ROOT 1

/* max nr of sectors of setup: don't change unless you also change
 * bootsect etc */
#define SETUP_SECTS 4

#define STRINGIFY(x) #x

void die(char * str)
{
	fprintf(stderr,"%s\n",str);
	exit(1);
}

void usage(void)
{
	die("Usage: build bootsect setup system [rootdev] [> image]");
}

int main(int argc, char ** argv)
{
	int i,c,id;
	char buf[1024];
	char major_root, minor_root;
	struct stat sb;
	fprintf(stderr,"********************************* .\n");

	fprintf(stderr,"argc is %d .\n",argc);
	for (int i = 0; i < argc; i++) {
		fprintf(stderr,"argc[%d] : %s .\n",i, argv[i]);
	}
	fprintf(stderr,"********************************* .\n");
	//if ((argc != 4) && (argc != 5))
		//usage();
	if (argc == 3) {
		if (strcmp(argv[2], "FLOPPY")) {
			if (stat(argv[2], &sb)) {
				perror(argv[2]);
				die("Couldn't stat root device.");
			}
			major_root = MAJOR(sb.st_rdev);
			minor_root = MINOR(sb.st_rdev);
		} else {
			major_root = 0;
			minor_root = 0;
		}
	} else {
		major_root = DEFAULT_MAJOR_ROOT;
		minor_root = DEFAULT_MINOR_ROOT;
	}
	fprintf(stderr, "Root device is (%d, %d)\n", major_root, minor_root);
	if ((major_root != 2) && (major_root != 3) &&
	    (major_root != 0)) {
		fprintf(stderr, "Illegal root device (major = %d)\n",
			major_root);
		die("Bad root device --- major #");
	}
	for (i=0;i<sizeof buf; i++) buf[i]=0;
	if ((id=open(argv[1],O_RDONLY,0))<0)
		die("Unable to open 'boot'");
	if (read(id,buf,MINIX_HEADER) != MINIX_HEADER)
		die("Unable to read header of 'boot'");
	if (((long *) buf)[0]!=0x04100301)
		die("Non-Minix header of 'boot'");
	if (((long *) buf)[1]!=MINIX_HEADER)
		die("Non-Minix header of 'boot'");
	if (((long *) buf)[3]!=0)
		die("Illegal data segment in 'boot'");
	if (((long *) buf)[4]!=0)
		die("Illegal bss in 'boot'");
	if (((long *) buf)[5] != 0)
		die("Non-Minix header of 'boot'");
	if (((long *) buf)[7] != 0)
		die("Illegal symbol table in 'boot'");
	i=read(id,buf,sizeof buf);
	fprintf(stderr,"Boot sector %d bytes.\n",i);
	if (i != 512)
		die("Boot block must be exactly 512 bytes");
	if ((*(unsigned short *)(buf+510)) != 0xAA55)
		die("Boot block hasn't got boot flag (0xAA55)");
	buf[508] = (char) minor_root;
	buf[509] = (char) major_root;	
	i=write(1,buf,512);
	if (i!=512)
		die("Write call failed");
	close (id);

	return(0);
}
