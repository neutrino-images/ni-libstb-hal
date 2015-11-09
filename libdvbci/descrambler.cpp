#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>

#include <linux/dvb/ca.h>

#include "misc.h"
#include "descrambler.h"

static const char * FILENAME = "[descrambler]";

/* alles im Kernel Header ca.h definiert da es der player2 auch braucht

enum ca_descr_data_type {
	CA_DATA_IV,
	CA_DATA_KEY,
};

enum ca_descr_parity {
	CA_PARITY_EVEN,
	CA_PARITY_ODD,
};

struct ca_descr_data {
	unsigned int index;
	enum ca_descr_parity parity;
	enum ca_descr_data_type data_type;
	unsigned int length;
	unsigned char *data;
};
#define CA_SET_DESCR_DATA _IOW('o', 137, struct ca_descr_data)

*/

static int desc_fd;

int descrambler_set_key(int index, int parity, unsigned char *data)
{
	struct ca_descr_data d;

	printf("%s -> %s\n", FILENAME, __FUNCTION__);
	d.index = index;
	d.parity = (ca_descr_parity)parity;
	d.data_type = CA_DATA_KEY;
	d.length = 16;
	d.data = data;

	printf("Index: %d Parity: %d Type CA_DATA_KEY= %d\n", d.index, d.parity, d.data_type);
	hexdump(d.data, 16);

	if (ioctl(desc_fd, CA_SET_DESCR_DATA, &d))
		printf("CA_SET_DESCR_DATA\n");

	d.index = index;
	d.parity = (ca_descr_parity)parity;
	d.data_type = CA_DATA_IV;
	d.length = 16;
	d.data = data + 16;

	printf("Index: %d Parity: %d Type CA_DATA_IV= %d\n", d.index, d.parity, d.data_type);
	hexdump(d.data, 16);

	if (ioctl(desc_fd, CA_SET_DESCR_DATA, &d))
		printf("CA_SET_DESCR_DATA\n");

	return 0;
}

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

int descrambler_init(void)
{
	const char *filename = "/dev/dvb/adapter0/ca1";

	printf("%s -> %s\n", FILENAME, __FUNCTION__);

	desc_fd = open(filename, O_RDWR | O_NONBLOCK );
	if (desc_fd <= 0) {
		printf("cannot open %s\n", filename);
		return -1;
	}

	return 0;
}

void descrambler_deinit(void)
{
	close(desc_fd);
}
