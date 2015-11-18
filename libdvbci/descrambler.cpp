#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>

#include <linux/dvb/ca.h>

#include "misc.h"
#include "descrambler.h"

static const char * FILENAME = "[descrambler]";

static const char *descrambler_filename = "/dev/dvb/adapter0/ca3";
static int desc_fd = -1;
static int desc_user_count = 0;

/* Byte 0 to 15 are AES Key, Byte 16 to 31 are IV */

int descrambler_set_key(int index, int parity, unsigned char *data)
{
	struct ca_descr_data d;

	printf("%s -> %s\n", FILENAME, __FUNCTION__);

	index |= 0x100;

	if (descrambler_open())
	{
		d.index = index;
		d.parity = (ca_descr_parity)parity;
		d.data_type = CA_DATA_KEY;
		d.length = 32;
		d.data = data;

		printf("Index: %d Parity: (%d) -> ", d.index, d.parity);
		hexdump(d.data, 32);

		if (ioctl(desc_fd, CA_SET_DESCR_DATA, &d))
		{
			//printf("CA_SET_DESCR_DATA\n");
		}
		descrambler_close();
	}
	return 0;
}

/* we don't use this for ci cam ! */
/*
int descrambler_set_pid(int index, int enable, int pid)
{
	struct ca_pid p;
	if (enable)
		p.index = index;
	else
		p.index = -1;

	p.pid = pid;

	if (ioctl(desc_fd, CA_SET_PID, &p))
		printf("CA_SET_PID\n");

	return 0;
}
*/

bool descrambler_open(void)
{
	desc_fd = open(descrambler_filename, O_RDWR | O_NONBLOCK );
	if (desc_fd <= 0) {
		printf("cannot open %s\n", descrambler_filename);
		return false;
	}
	return true;
}

void descrambler_close(void)
{
	close(desc_fd);
	desc_fd = -1;
}

int descrambler_init(void)
{
	desc_user_count++;
	printf("%s -> %s %d\n", FILENAME, __FUNCTION__, desc_user_count);
	return 0;
}

void descrambler_deinit(void)
{
	desc_user_count--;
	if (desc_user_count <= 0 && desc_fd > 0)
		descrambler_close();
}
