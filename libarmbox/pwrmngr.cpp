#include <stdio.h>

#include "pwrmngr.h"
#include "lt_debug.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_PWRMNGR, this, args)
void cCpuFreqManager::Up(void) { lt_debug("%s\n", __FUNCTION__); }
void cCpuFreqManager::Down(void) { lt_debug("%s\n", __FUNCTION__); }
void cCpuFreqManager::Reset(void) { lt_debug("%s\n", __FUNCTION__); }
/* those function dummies return true or "harmless" values */
bool cCpuFreqManager::SetDelta(unsigned long) { lt_debug("%s\n", __FUNCTION__); return true; }
unsigned long cCpuFreqManager::GetCpuFreq(void) { lt_debug("%s\n", __FUNCTION__); return 0; }
unsigned long cCpuFreqManager::GetDelta(void) { lt_debug("%s\n", __FUNCTION__); return 0; }
//
cCpuFreqManager::cCpuFreqManager(void) { lt_debug("%s\n", __FUNCTION__); }

bool cPowerManager::SetState(PWR_STATE) { lt_debug("%s\n", __FUNCTION__); return true; }

bool cPowerManager::Open(void) { lt_debug("%s\n", __FUNCTION__); return true; }
void cPowerManager::Close(void) { lt_debug("%s\n", __FUNCTION__); }
//
bool cPowerManager::SetStandby(bool Active, bool Passive) { lt_debug("%s(%d, %d)\n", __FUNCTION__, Active, Passive); return true; }
bool cCpuFreqManager::SetCpuFreq(unsigned long f) { lt_debug("%s(%lu) => set standby = %s\n", __FUNCTION__, f, f?"true":"false"); return true; }
//
cPowerManager::cPowerManager(void) { lt_debug("%s\n", __FUNCTION__); }
cPowerManager::~cPowerManager() { lt_debug("%s\n", __FUNCTION__); }

