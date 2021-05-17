/*
 * spark_fp - frontcontroller tool for SPARK boxes
 *
 * (C) 2012 Stefan Seyfried <seife@tuxboxcvs.slipkontur.de>
 *
 * License: GPL v2 or later
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <aotom_main.h>

#define FP_DEV "/dev/vfd"

void usage()
{
	printf("usage spark_fp <option>\n");
	printf("\t-g: get wakeup reason (return code == reason)\n");
	printf("\t-T: get FP display type (1 = VFD, 2 = LCD, 4 = LED, 8 = LBD)\n");
	printf("\t-t: get current FP time\n");
	printf("\t-s <time>: set FP time (time = 0: use current time)\n");
	printf("\t-w <time>: set FP wakeup time (time = 1: no wakeup)\n");
	printf("\t-P: power down\n");
	printf("\t-B <n>: show blue RC code (n = 0..4 or \"all\")\n");
	printf("\t-S <n>: show standby RC code (n = 0..4 or \"all\")\n");
	printf("\t-B <n>:<predata><code>: set blue RC code (n = 0..4)\n");
	printf("\t-S <n>:<predata><code>: set standby RC code\n");
	printf("\t-p: set LED flashing period (in ms)\n");
	printf("\t-l <n>: set LED <n> on\n");
	printf("\t-L <n>: set LED <n> off\n");
	printf("\t-i <n>: set icon <n> on\n");
	printf("\t-I <n>: set icon <n> off\n");
	printf("times are given in unix time (UTC, seconds since 1970-01-01 00:00:00)\n");
	printf("\n");
}

#if 0
double mod_julian_date(struct tm *t)
{
	double date;
	int month;
	int day;
	int year;

	year  = t->tm_year + 1900;
	month = t->tm_mon + 1;
	day   = t->tm_mday;

	date = day - 32076 +
	    1461 * (year + 4800 + (month - 14) / 12) / 4 +
	    367 * (month - 2 - (month - 14) / 12 * 12) / 12 -
	    3 * ((year + 4900 + (month - 14) / 12) / 100) / 4;

	date += (t->tm_hour + 12.0) / 24.0;
	date += (t->tm_min) / 1440.0;
	date += (t->tm_sec) / 86400.0;
	date -= 2400000.5;

	return date;
}
#endif

void time_to_aotom(time_t t, char *dest)
{
	/* from u-boot aotom */
	struct tm *tmp;
	tmp = localtime(&t);
#if 0
	/* this mjd stuff is totally useless: driver only uses dest[2] and dest[3]... */
	double mjd = mod_julian_date(tmp);
	int mjd_int = mjd;

	dest[0] = mjd_int >> 8;
	dest[1] = mjd_int & 0xff;
#else
	dest[0] = dest[1] = 0;
#endif
	dest[2] = tmp->tm_hour;
	dest[3] = tmp->tm_min;
	dest[4] = tmp->tm_sec;
}

int set_aotom_time(int fd, time_t t)
{
	struct tm *tmp;
	struct aotom_ioctl_data aotom;
	memset(&aotom, 0, sizeof(aotom));

	tmp = localtime(&t);
	t += tmp->tm_gmtoff;
	if (ioctl(fd, VFDSETTIME2, &t) >= 0)
		return 0;
	fprintf(stderr, "warning: VFDSETTIME2 failed (%m), falling back to VFDSETTIME\n");
	time_to_aotom(t, aotom.u.time.time);
	if (ioctl(fd, VFDSETTIME, &aotom) < 0)
	{
		perror("ioctl VFDSETTIME");
		return -1;
	}
	return 0;
}


int main(int argc, char **argv)
{
	const char *wakeupreason[4] = { "unknown", "poweron", "standby", "timer" };
	struct aotom_ioctl_data aotom;
	memset(&aotom, 0, sizeof(aotom));
	int ret, c, val;
	time_t t, t2, diff;
	struct tm *tmp;
	int period = LOG_ON;

	int fd = open(FP_DEV, O_RDWR);
	if (fd < 0)
	{
		fprintf(stderr, "error: cannot open %s: %m\n", FP_DEV);
		return 64;
	}

	if (argc == 1)
	{
		usage();
		return 0;
	}

	ret = 0;
	while ((c = getopt(argc, argv, "gs:tw:Tl:L:P:S:B:i:I:p:")) != -1)
	{
		switch (c)
		{
			case 'g':
				ret = ioctl(fd, VFDGETSTARTUPSTATE, &val);
				if (ret < 0)
					perror("ioctl VFDGETSTARTUPSTATE");
				else
				{
					printf("%s\n", wakeupreason[val & 0x03]);
					ret = val;
				}
				break;
			case 's':
				t = atol(optarg);
				if (t == 0)
					t = time(NULL),
					ret = set_aotom_time(fd, t);
				break;
			case 't':
				ret = ioctl(fd, VFDGETTIME, &t);
				if (ret < 0)
					perror("ioctl VFDGETTIME");
				else
				{
					tmp = localtime(&t);
					t -= tmp->tm_gmtoff;
					printf("%ld\n", t);
				}
				break;
			case 'w':
				t = atol(optarg);
				if (t == 0)
				{
					fprintf(stderr, "invalid time?\n");
					ret = 1;
					break;
				}
				/* set AOTOM time to current time... */
				t2 = time(NULL);
				ret = set_aotom_time(fd, t2);
				if (ret < 0)
					break;
				diff = t - t2;
				if (t == 1)
					t = 0; /* t = 1 is magic for "no time" -> clear... */
				else
				{
					/* green LED on */
					aotom.u.led.on = LOG_ON;
					aotom.u.led.led_nr = 1;
					ioctl(fd, VFDSETLED, &aotom);
					ret = ioctl(fd, VFDGETTIME, &t);
					if (ret < 0)
					{
						perror("ioctl VFDGETTIME");
						break;
					}
					if (t < t2 + 20)
						diff = 20;
					t += diff;
				}
				tmp = gmtime(&t2);
				fprintf(stderr, "current time: %04d-%02d-%02d %02d:%02d:%02d\n", tmp->tm_year + 1900,
				    tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
				tmp = gmtime(&t);
				fprintf(stderr, "wakeup time:  %04d-%02d-%02d %02d:%02d:%02d\n", tmp->tm_year + 1900,
				    tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
				ret = ioctl(fd, VFDSETPOWERONTIME, &t);
				break;
			case 'P':
				ret = ioctl(fd, VFDPOWEROFF);
				break;
			case 'p':
				period = atoi(optarg) / 10;
				break;
			case 'l': /* LED on */
				aotom.u.led.on = period;
				aotom.u.led.led_nr = atoi(optarg);
				ioctl(fd, VFDSETLED, &aotom);
				break;
			case 'L': /* LED off */
				aotom.u.led.on = LOG_OFF;
				aotom.u.led.led_nr = atoi(optarg);
				ioctl(fd, VFDSETLED, &aotom);
				break;
			case 'i': /* icon on */
				aotom.u.icon.on = 1;
				aotom.u.icon.icon_nr = atoi(optarg);
				ioctl(fd, VFDICONDISPLAYONOFF, &aotom);
				break;
			case 'I': /* icon off */
				aotom.u.icon.on = 0;
				aotom.u.icon.icon_nr = atoi(optarg);
				ioctl(fd, VFDICONDISPLAYONOFF, &aotom);
				break;
			case 'T':
				ret = ioctl(fd, VFDGETVERSION, &val);
				if (ret < 0)
					perror("ioctl VFDGETVERSION");
				else
				{
					printf("%d\n", val);
					ret = val;
				}
				break;
// Reminder to myself, here's a semi-sane default for Pingulux boxes:
// spark_fp -S 0:9966da25 -S 1:11eeda25 -S 2:cc33ba45 -S 3:dd227887 -S 4:aa557887 -B 0:996640bf -B 1:11ee40bf -B 2:cc33b847 -B 3:dd2228d7 -B 4:aa5528d7
// Not sure whether these are the original settings. --martii

			case 'S':
				if (2 == sscanf(optarg, "%d:%lx", &aotom.u.key.key_nr, (long unsigned int *) &aotom.u.key.key))
					ioctl(fd, VFDSETSTBYKEY, &aotom);
				if (1 == sscanf(optarg, "%d", &aotom.u.key.key_nr))
				{
					ret = ioctl(fd, VFDGETSTBYKEY, &aotom);
					if (ret)
						perror("ioctl VFDGETSTBYKEY");
					else
						fprintf(stderr, "stby key %d = %.8x\n", aotom.u.key.key_nr, aotom.u.key.key);
				}
				else
				{
					aotom.u.key.key_nr = 0;
					while (aotom.u.key.key_nr < 5)
					{
						ret = ioctl(fd, VFDGETSTBYKEY, &aotom);
						if (ret)
							perror("ioctl VFDGETSTBYKEY");
						else
							fprintf(stderr, "stby key %d = %.8x\n", aotom.u.key.key_nr, aotom.u.key.key);
						aotom.u.key.key_nr++;
					}
				}
				break;
			case 'B':
				if (2 == sscanf(optarg, "%d:%lx", &aotom.u.key.key_nr, (long unsigned int *) &aotom.u.key.key))
					ioctl(fd, VFDSETBLUEKEY, &aotom);
				if (1 == sscanf(optarg, "%d", &aotom.u.key.key_nr))
				{
					ret = ioctl(fd, VFDGETBLUEKEY, &aotom);
					if (ret)
						perror("ioctl VFDGETBLUEKEY");
					else
						fprintf(stderr, "blue key %d = %.8x\n", aotom.u.key.key_nr, aotom.u.key.key);
				}
				else
				{
					aotom.u.key.key_nr = 0;
					while (aotom.u.key.key_nr < 5)
					{
						ret = ioctl(fd, VFDGETBLUEKEY, &aotom);
						if (ret)
							perror("ioctl VFDGETBLUEKEY");
						else
							fprintf(stderr, "blue key %d = %.8x\n", aotom.u.key.key_nr, aotom.u.key.key);
						aotom.u.key.key_nr++;
					}
				}
				break;
			default:
				usage();
				return 0;
		}
	}
	close(fd);
	return ret;
}
