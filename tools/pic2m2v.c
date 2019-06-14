/*
 * (C) 2012 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>

#if HAVE_TRIPLEDRAGON
#define TARGETRES "704x576"
#else
#define TARGETRES "1280x720"
#endif

int main(int argc, char **argv)
{
	char destname[448];
	char cmd[512];
	char *p;
	char *fname;
	struct stat st, st2;

	if (argc < 2)
	{
		fprintf(stderr, "usage: pic2m2v /path/pic1.jpg [/path/pic2.jpg...]\n\n");
		return 1;
	}
	strcpy(destname, "/var/cache");
	mkdir(destname, 0755);
	argv++;
	for (fname = *argv; fname != NULL; fname = *++argv)
	{
		if (stat(fname, &st2))
		{
			fprintf(stderr, "pic2m2v: could not stat '%s' (%m)\n", fname);
			continue;
		}
		strcpy(destname, "/var/cache");
		/* the cache filename is (example for /share/tuxbox/neutrino/icons/radiomode.jpg):
		   /var/cache/share.tuxbox.neutrino.icons.radiomode.jpg.m2v
		   build that filename first...
		   TODO: this could cause name clashes, use a hashing function instead... */
		strcat(destname, fname);
		p = &destname[strlen("/var/cache/")];
		while ((p = strchr(p, '/')) != NULL)
			*p = '.';
		strcat(destname, ".m2v");
		/* ...then check if it exists already... */
		if (stat(destname, &st) || (st.st_mtime != st2.st_mtime) || (st.st_size == 0))
		{
			struct utimbuf u;
			u.actime = time(NULL);
			u.modtime = st2.st_mtime;
			printf("converting %s -> %s\n", fname, destname);
			/* it does not exist or has a different date, so call ffmpeg... */
			sprintf(cmd, "ffmpeg -y -f mjpeg -i '%s' -s %s '%s' </dev/null",
							fname, TARGETRES, destname);
			system(cmd); /* TODO: use libavcodec to directly convert it */
			utime(destname, &u);
		}
		else
			printf("cache file %s already current\n", destname);
	}

	return 0;
}
