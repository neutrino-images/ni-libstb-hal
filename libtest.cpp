/* minimal test program for libstb-hal
 * (C) 2012 Stefan Seyfried
 * License: GPL v2 or later
 *
 * this does just test the input converter thread for now...
 */

#include <config.h>
#include <unistd.h>
#include <include/init_td.h>

int main(int argc, char ** argv)
{
	init_td_api();
	while (1) {
		sleep(1);
		if (! access("/tmp/endtest", R_OK))
		{
			unlink("/tmp/endtest");
			break;
		}
	};
	shutdown_td_api();
	return 0;
}
