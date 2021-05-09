#ifndef __VERSION_HAL_H__
#define __VERSION_HAL_H__

#include <string>

// library version functions
typedef struct hal_libversion_t
{
	std::string	vVersion;
	int			vMajor;
	int			vMinor;
	int			vPatch;;
	std::string	vName;
	std::string	vStr;
	std::string	vGitDescribe;
} hal_libversion_struct_t;

void hal_get_lib_version(hal_libversion_t *ver);

#endif //__VERSION_HAL_H__
