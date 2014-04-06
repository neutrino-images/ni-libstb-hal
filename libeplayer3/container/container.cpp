/*
 * Main Container Handling.
 *
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdio.h>
#include <string.h>

#include "common.h"

#define CONTAINER_DEBUG

#ifdef CONTAINER_DEBUG

static short debug_level = 0;

#define container_printf(level, x...) do { \
if (debug_level >= level) printf(x); } while (0)
#else
#define container_printf(level, x...)
#endif

#ifndef CONTAINER_SILENT
#define container_err(x...) do { printf(x); } while (0)
#else
#define container_err(x...)
#endif

static int Command(Player *context, ContainerCmd_t command, const char *argument __attribute__((unused)))
{
    int ret = 0;

    container_printf(10, "%s::%s\n", __FILE__, __func__);

    switch (command) {
    case CONTAINER_ADD:
	    context->container->selectedContainer = &FFMPEGContainer;
	    break;
    case CONTAINER_DEL:{
	    context->container->selectedContainer = NULL;
	    break;
	}
    default:
	container_err("%s::%s ContainerCmd %d not supported!\n", __FILE__,
		      __func__, command);
	ret = -1;
	break;
    }
    return ret;
}

ContainerHandler_t ContainerHandler = {
    "Output",
    NULL,
    Command
};
