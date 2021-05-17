/*
 * (C) 2018 Thilo Graf
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <libstb-hal-config.h>

#include <version_hal.h>


void hal_get_lib_version(hal_libversion_t *ver)
{
	if (!ver)
		return;

	//init struct
	*ver = {"", 0, 0, 0, "", "", ""};

#ifdef VERSION
	ver->vVersion = VERSION;
#endif
#ifdef PACKAGE_VERSION_MAJOR
	ver->vMajor = PACKAGE_VERSION_MAJOR;
#endif
#ifdef PACKAGE_VERSION_MAJOR
	ver->vMinor = PACKAGE_VERSION_MINOR;
#endif
#ifdef PACKAGE_VERSION_MINOR
	ver->vPatch = PACKAGE_VERSION_MICRO;
#endif
#ifdef PACKAGE_NAME
	ver->vName = PACKAGE_NAME;
#endif
#ifdef PACKAGE_STRING
	ver->vStr = PACKAGE_STRING;
#endif
#ifdef PACKAGE_VERSION_GIT
	ver->vGitDescribe = PACKAGE_VERSION_GIT;
#endif
}
