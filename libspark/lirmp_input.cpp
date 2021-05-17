/*
 * Simulate a linux input device via uinput
 * Get lirc remote events, decode with IRMP and inject them via uinput
 *
 * (C) 2012 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* the C++ compiler did not like this code, so let's put it into a
 * separate file and compile with gcc insead of g++...
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#include <linux/ioctl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <inttypes.h>
#include <errno.h>

#include <aotom_main.h>

#include "lirmp_input.h"
extern "C" {
#include "irmp.h"
}
static uint8_t IRMP_PIN;

#include <hal_debug.h>
#define hal_debug(args...) _hal_debug(HAL_DEBUG_INIT, NULL, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_INIT, NULL, args)

/* same defines as in neutrino's rcinput.h */
#define KEY_TTTV    KEY_FN_1
#define KEY_TTZOOM  KEY_FN_2
#define KEY_REVEAL  KEY_FN_D
/* only defined in newer kernels / headers... */
#ifndef KEY_ZOOMIN
#define KEY_ZOOMIN  KEY_FN_E
#endif
#ifndef KEY_ZOOMOUT
#define KEY_ZOOMOUT KEY_FN_F
#endif

typedef struct
{
	uint16_t ir;    /* IR command */
	int code;   /* input key code */
} key_map_t;

static const key_map_t key_map[] =
{
	{ 0x13, KEY_0 },
	{ 0x1a, KEY_1 },
	{ 0x1f, KEY_2 },
	{ 0x58, KEY_3 },
	{ 0x16, KEY_4 },
	{ 0x1b, KEY_5 },
	{ 0x54, KEY_6 },
	{ 0x12, KEY_7 },
	{ 0x17, KEY_8 },
	{ 0x50, KEY_9 },
	{ 0x5f, KEY_OK },
	{ 0x59, KEY_TIME },
	{ 0x43, KEY_FAVORITES },
	{ 0x4f, KEY_SAT },
	{ 0x0f, KEY_NEXT }, /* V.Format */
	{ 0x1e, KEY_POWER },
	{ 0x5a, KEY_MUTE },
	{ 0x1c, KEY_MENU },
	{ 0x5d, KEY_EPG },
	{ 0x07, KEY_INFO },
	{ 0x60, KEY_EXIT },
	{ 0x48, KEY_PAGEUP },
	{ 0x44, KEY_PAGEDOWN },
	{ 0x02, KEY_LEFT },
	{ 0x40, KEY_RIGHT },
	{ 0x03, KEY_UP },
	{ 0x5e, KEY_DOWN },
	{ 0x0a, KEY_VOLUMEUP },
	{ 0x06, KEY_VOLUMEDOWN },
	{ 0x49, KEY_RED },
	{ 0x4e, KEY_GREEN },
	{ 0x11, KEY_YELLOW },
	{ 0x4a, KEY_BLUE },
	{ 0x4c, KEY_TV },   /* TV/Radio */
	{ 0x5c, KEY_VIDEO },    /* FIND */
	{ 0x19, KEY_AUDIO },    /* FOLDER */
	/*  KEY_AUX,
	    KEY_TEXT,
	    KEY_TTTV,
	    KEY_TTZOOM,
	    KEY_REVEAL,
	*/
	{ 0x01, KEY_REWIND },
	{ 0x53, KEY_FORWARD },
	{ 0x22, KEY_STOP },
	{ 0x4d, KEY_PAUSE },
	{ 0x15, KEY_PLAY },
	{ 0x20, KEY_PREVIOUS },
	{ 0x23, KEY_NEXT },
//	KEY_EJECTCD,
	{ 0x10, KEY_RECORD }
};

static const int key_list[] =
{
	KEY_0,
	KEY_1,
	KEY_2,
	KEY_3,
	KEY_4,
	KEY_5,
	KEY_6,
	KEY_7,
	KEY_8,
	KEY_9,
	KEY_OK,
	KEY_TIME,
	KEY_FAVORITES,
	KEY_SAT,
	KEY_ZOOMOUT,
	KEY_ZOOMIN,
	KEY_NEXT,
	KEY_POWER,
	KEY_MUTE,
	KEY_MENU,
	KEY_EPG,
	KEY_INFO,
	KEY_EXIT,
	KEY_PAGEUP,
	KEY_PAGEDOWN,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_UP,
	KEY_DOWN,
	KEY_VOLUMEUP,
	KEY_VOLUMEDOWN,
	KEY_RED,
	KEY_GREEN,
	KEY_YELLOW,
	KEY_BLUE,
	KEY_TV,
	KEY_VIDEO,
	KEY_AUDIO,
//	KEY_AUX,
//	KEY_TEXT,
//	KEY_TTTV,
//	KEY_TTZOOM,
//	KEY_REVEAL,
	KEY_REWIND,
	KEY_STOP,
	KEY_PAUSE,
	KEY_PLAY,
	KEY_FORWARD,
	KEY_PREVIOUS,
	KEY_NEXT,
//	KEY_EJECTCD,
	KEY_RECORD,
	-1
};

static pthread_t thread;
static int thread_running;

static void *input_thread(void *)
{
	int uinput;
	struct input_event u;
	struct uinput_user_dev ud;
	FILE *f;
	int lircfd;
	int pulse;
	int i = 0;
	int last_pulse = 1;
	int last_code = -1;
	uint32_t lircdata;  /* lirc_t to be correct... */
	unsigned int count = 0; /* how many timeouts? */
	unsigned int nodec = 0; /* how many timeouts since last decoded? */
	int aotom_fd = -1;
	IRMP_DATA d;

	hal_info("LIRC/IRMP input converter thread starting...\n");

	/* modprobe does not complain if the module is already loaded... */
	system("/sbin/modprobe uinput");
	do
	{
		usleep(100000); /* mdev needs some time to create the device? */
		uinput = open("/dev/uinput", O_WRONLY | O_NDELAY);
	}
	while (uinput < 0 && ++count < 100);

	if (uinput < 0)
	{
		hal_info("LIRC/IRMP input thread: unable to open /dev/uinput (%m)\n");
		thread_running = 2;
		return NULL;
	}

	fcntl(uinput, F_SETFD, FD_CLOEXEC);
	ioctl(uinput, UI_SET_EVBIT, EV_KEY);
	/* do not use kernel repeat EV_REP since neutrino will be confused by the
	 * generated SYN_REPORT events...
	ioctl(uinput, UI_SET_EVBIT, EV_REP);
	 */
	/* register keys */
	for (i = 0; key_list[i] != -1; i++)
		ioctl(uinput, UI_SET_KEYBIT, key_list[i]);

	/* configure the device */
	memset(&ud, 0, sizeof(ud));
	strncpy(ud.name, "Neutrino LIRC/IRMP to Input Device converter", UINPUT_MAX_NAME_SIZE);
	ud.id.version = 0x42;
	ud.id.vendor  = 0x1234;
	ud.id.product = 0x5678;
	ud.id.bustype = BUS_I2C; /* ?? */
	write(uinput, &ud, sizeof(ud));

	if (ioctl(uinput, UI_DEV_CREATE))
	{
		hal_info("LIRC/IRMP input thread UI_DEV_CREATE: %m\n");
		close(uinput);
		return NULL;
	}

	/* this is ugly: parse the new input device from /proc/...devices
	 * and symlink it to /dev/input/nevis_ir... */
#define DEVLINE "I: Bus=0018 Vendor=1234 Product=5678 Version=0042"
	f = fopen("/proc/bus/input/devices", "r");
	if (f)
	{
		int found = 0;
		int evdev = -1;
		size_t n = 0;
		char *line = NULL;
		char *p;
		char newdev[20];
		while (getline(&line, &n, f) != -1)
		{
			switch (line[0])
			{
				case 'I':
					if (strncmp(line, DEVLINE, strlen(DEVLINE)) == 0)
						found = 1;
					break;
				case 'H':
					if (! found)
						break;
					p = strstr(line, " event");
					if (! p)
					{
						evdev = -1;
						break;
					}
					evdev = atoi(p + 6);
					sprintf(newdev, "event%d", evdev);
					hal_info("LIRC/IRMP input thread: symlink /dev/input/nevis_ir to %s\n", newdev);
					unlink("/dev/input/nevis_ir");
					symlink(newdev, "/dev/input/nevis_ir");
					break;
				default:
					break;
			}
			if (evdev != -1)
				break;
		}
		fclose(f);
		free(line);
	}

	u.type = EV_KEY;
	u.value = 0; /* initialize: first event wil be a key press */

	lircfd = open("/dev/lirc", O_RDONLY);
	if (lircfd < 0)
	{
		hal_info("%s: open /dev/lirc: %m\n", __func__);
		goto out;
	}
	IRMP_PIN = 0xFF;

	/* 50 ms. This should be longer than the longest light pulse */
#define POLL_MS     (100 * 1000)
#define LIRC_PULSE  0x01000000
#define LIRC_PULSE_MASK 0x00FFFFFF
	hal_info("LIRC/IRMP input converter going into main loop...\n");

	aotom_fd = open("/dev/vfd", O_RDONLY);

	/* TODO: ioctl to find out if we have a compatible LIRC_MODE2 device */
	thread_running = 1;
	while (thread_running)
	{
		fd_set fds;
		struct timeval tv;
		int ret;

		FD_ZERO(&fds);
		FD_SET(lircfd, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = POLL_MS;
		/* any singal can interrupt select. we rely on the linux-only feature
		 * that the timeout is automatcally recalculated in this case! */
		do
		{
			ret = select(lircfd + 1, &fds, NULL, NULL, &tv);
		}
		while (ret == -1 && errno == EINTR);

		if (ret == -1)
		{
			/* errno != EINTR... */
			hal_info("%s: lirmp: lircfd select: %m\n", __func__);
			break;
		}

		if (ret == 0)
		{
			count++;
			nodec++;
			lircdata = POLL_MS; /* timeout */
			pulse = !last_pulse;    /* lirc sends data on signal change */
			if (last_code != -1 && nodec > 1)
			{
				// fprintf(stderr, "timeout!\n");
				u.code = last_code;
				u.value = 0;    /* release */
				write(uinput, &u, sizeof(u));
				last_code = -1;
			}
		}
		else
		{
			if (read(lircfd, &lircdata, sizeof(lircdata)) != sizeof(lircdata))
			{
				perror("read");
				break;
			}
			pulse = (lircdata & LIRC_PULSE);    /* we got light... */
			last_pulse = pulse;
			lircdata &= LIRC_PULSE_MASK;        /* how long the pulse was in microseconds */
		}

		if (ret && count)
		{
			if (count * POLL_MS > lircdata)
				lircdata = 0;
			else
				lircdata -= count * POLL_MS;
			count = 0;
		}
		//printf("lircdata: ret:%d c:%d %d\n", ret, ch - '0', lircdata);
		lircdata /= (1000000 / F_INTERRUPTS);

		if (pulse)
			IRMP_PIN = 0x00;
		else
			IRMP_PIN = 0xff;

		do
		{
			(void) irmp_ISR(IRMP_PIN);
			if (irmp_get_data(&d))
			{
				nodec = 0;
				hal_debug("irmp_get_data proto: %2d addr: 0x%04x cmd: 0x%04x fl: %d\n",
				    d.protocol, d.address, d.command, d.flags);

				/* todo: do we need to complete the loop if we already
				 * detected the singal in this pulse? */
				if (d.protocol == IRMP_NEC_PROTOCOL && d.address == 0xba45)
				{
					for (i = 0; i < (int)(sizeof(key_map) / sizeof(key_map_t)); i++)
					{
						if (key_map[i].ir == d.command)
						{
							if (last_code != -1 && last_code != key_map[i].code)
							{
								u.code = last_code;
								u.value = 0;
								write(uinput, &u, sizeof(u));
							}
							u.code = key_map[i].code;
							u.value = (d.flags & 0x1) + 1;
							//hal_debug("uinput write: value: %d code: %d\n", u.value, u.code);
							last_code = u.code;
							write(uinput, &u, sizeof(u));
							if (aotom_fd > -1)
							{
								struct aotom_ioctl_data vfd_data;
								vfd_data.u.led.led_nr = 1;
								vfd_data.u.led.on = 10;
								ioctl(aotom_fd, VFDSETLED, &vfd_data);
							}
							break;
						}
					}
				}
			}
		}
		while (lircdata-- > 0);
	}
	/* clean up */
	close(lircfd);

	if (aotom_fd > -1)
		close(aotom_fd);

out:
	ioctl(uinput, UI_DEV_DESTROY);
	return NULL;
}

void start_input_thread(void)
{
	if (pthread_create(&thread, 0, input_thread, NULL) != 0)
	{
		hal_info("%s: LIRC/IRMP input thread pthread_create: %m\n", __func__);
		thread_running = 0;
		return;
	}
	/* wait until the device is created before continuing */
	while (! thread_running)
		usleep(1000);
	if (thread_running == 2) /* failed... :-( */
		thread_running = 0;
}

void stop_input_thread(void)
{
	if (! thread_running)
		return;
	thread_running = 0;
	pthread_join(thread, NULL);
}
