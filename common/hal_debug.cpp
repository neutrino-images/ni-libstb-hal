/* libstb-hal debug functions */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <string.h>


int cnxt_debug = 0; /* compat, unused */

int debuglevel = -1;

static const char* hal_facility[] = {
	"audio ",
	"video ",
	"demux ",
	"play  ",
	"power ",
	"init  ",
	"ca    ",
	"record",
	NULL
};

void _hal_info(int facility, const void *func, const char *fmt, ...)
{
	/* %p does print "(nil)" instead of 0x00000000 for NULL */
	fprintf(stderr, "[HAL:%08lx:%s] ", (long) func, hal_facility[facility]);
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}


void _hal_debug(int facility, const void *func, const char *fmt, ...)
{
	if (debuglevel < 0)
		fprintf(stderr, "hal_debug: debuglevel not initialized!\n");

	if (! ((1 << facility) & debuglevel))
		return;

	fprintf(stderr, "[HAL:%08lx:%s] ", (long)func, hal_facility[facility]);
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void hal_debug_init(void)
{
	int i = 0;
	char *tmp = getenv("HAL_DEBUG");
	if (! tmp)
		tmp = getenv("TRIPLE_DEBUG"); /* backwards compatibility... */
	if (! tmp)
		debuglevel = 0;
	else
		debuglevel = (int) strtol(tmp, NULL, 0);

	if (debuglevel == 0)
	{
		fprintf(stderr, "libstb-hal debug options can be set by exporting HAL_DEBUG.\n");
		fprintf(stderr, "The following values (or bitwise OR combinations) are valid:\n");
		while (hal_facility[i]) {
			fprintf(stderr, "\tcomponent: %s  0x%02x\n", hal_facility[i], 1 << i);
			i++;
		}
		fprintf(stderr, "\tall components:    0x%02x\n", (1 << i) - 1);
	} else {
		fprintf(stderr, "libstb-hal debug is active for the following components:\n");
		while (hal_facility[i]) {
			if (debuglevel & (1 << i))
				fprintf(stderr, "%s ", hal_facility[i]);
			i++;
		}
		fprintf(stderr, "\n");
	}
}

void hal_set_threadname(const char *name)
{
	char threadname[17];
	strncpy(threadname, name, sizeof(threadname));
	threadname[16] = 0;
	prctl (PR_SET_NAME, (unsigned long)&threadname);
}
