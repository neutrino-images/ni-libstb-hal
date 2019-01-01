#ifndef __VERSION_HAL_H__
#define __VERSION_HAL_H__

#include <string>

std::string getPackageVersion();
int getPackageVersionMajor();
int getPackageVersionMinor();
int getPackageVersionMicro();
std::string getPackagenName();
std::string getPackageString();
std::string getPackageVersionGit();


#endif //__VERSION_HAL_H__

