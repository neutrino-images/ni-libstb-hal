
#include <config.h>

#include <stdio.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/dvb/dmx.h>

#include "init.h"
#include "pwrmngr.h"

#include "hal_debug.h"
#define hal_debug(args...) _hal_debug(HAL_DEBUG_INIT, NULL, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_INIT, NULL, args)

#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <linux/input.h>
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <string.h>

#define VIRTUALINPUT "/sys/devices/virtual/input"
#define DEVINPUT "/dev/input"

typedef struct
{
	const char *name;
	const char *desc;
	int fd;
	unsigned int major;
	unsigned int minor;
	time_t next_discovery;
} input_device_t;

static input_device_t input_device[] =
{
	{ "/dev/input/nevis_ir",    "lircd",            -1, 0, 0, 0 },
	{ "/dev/input/tdt_rc",      "TDT RC event driver",      -1, 0, 0, 0 },
	{ "/dev/input/fulan_fp",    "fulan front panel buttons",    -1, 0, 0, 0 },
	{ "/dev/input/event0",      NULL,               -1, 0, 0, 0 },
	{ "/dev/input/event1",      NULL,               -1, 0, 0, 0 },
	{ "/dev/input/event2",      NULL,               -1, 0, 0, 0 },
	{ "/dev/input/event3",      NULL,               -1, 0, 0, 0 },
	{ "/dev/input/event4",      NULL,               -1, 0, 0, 0 },
	{ "/dev/input/event5",      NULL,               -1, 0, 0, 0 },
	{ "/dev/input/event6",      NULL,               -1, 0, 0, 0 },
	{ "/dev/input/event7",      NULL,               -1, 0, 0, 0 },
	{ NULL,             NULL,               -1, 0, 0, 0 }
};

#define number_of_input_devices (sizeof(input_device)/sizeof(input_device_t) - 1)

static void do_mknod(int i, char *d_name)
{
	char name[255];
	int dev = -1;
	// I've no idea how the event device number is actually calculated. Just loop.  --martii

	for (int j = 0; j < 99 && dev < 0; j++)
	{
		snprintf(name, sizeof(name), VIRTUALINPUT "/%s/event%d/dev", d_name, j);
		dev = open(name, O_RDONLY);
	}

	if (dev > -1)
	{
		char buf[255];
		int l = read(dev, buf, sizeof(buf) - 1);
		close(dev);
		if (l > -1)
		{
			buf[l] = 0;
			if (2 == sscanf(buf, "%d:%d", &input_device[i].major, &input_device[i].minor))
			{
				mknod(input_device[i].name, 0666 | S_IFCHR,
				    gnu_dev_makedev(input_device[i].major, input_device[i].minor));
			}
		}
	}
}

static void create_input_devices(void)
{
	DIR *d = opendir(VIRTUALINPUT);
	if (d)
	{
		struct dirent *e;
		while ((e = readdir(d)))
		{
			char name[255];
			if (e->d_name[0] == '.')
				continue;
			snprintf(name, sizeof(name), VIRTUALINPUT "/%s/name", e->d_name);
			int n = open(name, O_RDONLY);
			if (n > -1)
			{
				char buf[255];
				int l = read(n, buf, sizeof(buf) - 1);
				close(n);
				if (l > 1)
				{
					do
						buf[l--] = 0;
					while (l > 1 && buf[l] == '\n');

					for (unsigned int i = 0; i < number_of_input_devices; i++)
						if (input_device[i].desc && !strcmp(buf, input_device[i].desc))
						{
							do_mknod(i, e->d_name);
							break;
						}
				}
			}
		}
		closedir(d);
	}
	// remove any event* files left that point to our "well-known" inputs
	d = opendir(DEVINPUT);
	if (d)
	{
		struct dirent *e;
		while ((e = readdir(d)))
		{
			char name[255];
			if (strncmp(e->d_name, "event", 5))
				continue;
			snprintf(name, sizeof(name), DEVINPUT "/%s", e->d_name);
			struct stat st;
			if (stat(name, &st))
				continue;
			for (unsigned int i = 0; i < number_of_input_devices; i++)
				if (input_device[i].major &&
				    gnu_dev_major(st.st_rdev) == input_device[i].major &&
				    gnu_dev_minor(st.st_rdev) == input_device[i].minor)
					unlink(name);
		}
		closedir(d);
	}
}

static pthread_t inmux_task = 0;
static int inmux_thread_running = 0;

static void open_input_devices(void)
{
	time_t now = time(NULL);
	for (unsigned int i = 0; i < number_of_input_devices; i++)
		if ((input_device[i].fd < 0) && (input_device[i].next_discovery <= now))
		{
			input_device[i].next_discovery = now + 60;
			input_device[i].fd = open(input_device[i].name, O_RDWR | O_NONBLOCK);
		}
}

static void reopen_input_devices(void)
{
	create_input_devices();
	time_t now = time(NULL);
	for (unsigned int i = 0; i < number_of_input_devices; i++)
	{
		input_device[i].next_discovery = now + 60;
		int fd = open(input_device[i].name, O_RDWR | O_NONBLOCK);
		if (fd > -1)
		{
			if (input_device[i].fd > -1)
			{
				dup2(fd, input_device[i].fd);
				close(fd);
			}
			else
			{
				input_device[i].fd = fd;
			}
		}
		else if (input_device[i].fd > -1)
		{
			close(input_device[i].fd);
			input_device[i].fd = -1;
		}
	}
}

static void close_input_devices(void)
{
	for (unsigned int i = 0; i < number_of_input_devices; i++)
		if (input_device[i].fd > -1)
		{
			close(input_device[i].fd);
			input_device[i].fd = -1;
		}
}

static void poll_input_devices(void)
{
	struct pollfd fds[number_of_input_devices];
	input_device_t *inputs[number_of_input_devices];
	int nfds = 0;
	for (unsigned int i = 1; i < number_of_input_devices; i++)
		if (input_device[i].fd > -1)
		{
			fds[nfds].fd = input_device[i].fd;
			fds[nfds].events = POLLIN | POLLHUP | POLLERR;
			fds[nfds].revents = 0;
			inputs[nfds] = &input_device[i];
			nfds++;
		}

	if (nfds == 0)
	{
		// Only a single input device, which happens to be our master. poll() to avoid looping too fast.
		fds[0].fd = input_device[0].fd;
		fds[0].events = POLLIN | POLLHUP | POLLERR;
		fds[0].revents = 0;
		poll(fds, 1, 60000 /* ms */);
		return;
	}

	int r = poll(fds, nfds, 60000 /* ms */);
	if (r < 0)
	{
		if (errno != EAGAIN)
		{
			hal_info("%s: poll(): %m\n", __func__);
			inmux_thread_running = 0;
		}
		return;
	}
	for (int i = 0; i < nfds && r > 0; i++)
	{
		if (fds[i].revents & POLLIN)
		{
//fprintf(stderr, "### input from fd %d (%s)\n", fds[i].fd, inputs[i]->name);
			struct input_event ev;
			while (sizeof(ev) == read(fds[i].fd, &ev, sizeof(ev)))
				write(input_device[0].fd, &ev, sizeof(ev));
			r--;
		}
		else if (fds[i].revents & (POLLHUP | POLLERR | POLLNVAL))
		{
//fprintf(stderr, "### error on %d (%s)\n", fds[i].fd, inputs[i]->name);
			close(fds[i].fd);
			inputs[i]->fd = -1;
			r--;
		}
	}
}

static void *inmux_thread(void *)
{
	char threadname[17];
	strncpy(threadname, __func__, sizeof(threadname));
	threadname[16] = 0;
	prctl(PR_SET_NAME, (unsigned long)&threadname);

	inmux_thread_running = 1;
	while (inmux_thread_running)
	{
		open_input_devices();
		poll_input_devices();
	}

	return NULL;
}

void start_inmux_thread(void)
{
	input_device[0].fd = open(input_device[0].name, O_RDWR | O_NONBLOCK); // nevis_ir. This is mandatory.
	if (input_device[0].fd < 0)
	{
		hal_info("%s: open(%s): %m\n", __func__, input_device[0].name);
		return;
	}
	if (pthread_create(&inmux_task, 0, inmux_thread, NULL) != 0)
	{
		hal_info("%s: inmux thread pthread_create: %m\n", __func__);
		inmux_thread_running = 0;
		return;
	}
	pthread_detach(inmux_task);
}

void stop_inmux_thread(void)
{
	inmux_thread_running = 0;
}

static bool initialized = false;

void hal_api_init()
{
	if (!initialized)
		hal_debug_init();
	hal_info("%s begin, initialized=%d, debug=0x%02x\n", __FUNCTION__, (int)initialized, debuglevel);
	if (!initialized)
	{
		cCpuFreqManager f;
		f.SetCpuFreq(0);    /* CPUFREQ == 0 is the trigger for leaving standby */
		create_input_devices();
		start_inmux_thread();

		/* this is a strange hack: the drivers seem to only work correctly after
		 * demux0 has been used once. After that, we can use demux1,2,... */
		struct dmx_pes_filter_params p;
		int dmx = open("/dev/dvb/adapter0/demux0", O_RDWR | O_CLOEXEC);
		if (dmx < 0)
			hal_info("%s: ERROR open /dev/dvb/adapter0/demux0 (%m)\n", __func__);
		else
		{
			memset(&p, 0, sizeof(p));
			p.output = DMX_OUT_DECODER;
			p.input  = DMX_IN_FRONTEND;
			p.flags  = DMX_IMMEDIATE_START;
			p.pes_type = DMX_PES_VIDEO;
			ioctl(dmx, DMX_SET_PES_FILTER, &p);
			ioctl(dmx, DMX_STOP);
			close(dmx);
		}
	}
	else
		reopen_input_devices();
	initialized = true;
	hal_info("%s end\n", __FUNCTION__);
}

void hal_api_exit()
{
	hal_info("%s, initialized = %d\n", __FUNCTION__, (int)initialized);
	if (initialized)
	{
		stop_inmux_thread();
		close_input_devices();
	}
	initialized = false;
}
