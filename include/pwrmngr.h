/*
 * (C) 2010-2013 Stefan Seyfried
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

#ifndef __PWRMNGR_H__
#define __PWRMNGR_H__

class cCpuFreqManager
{
	public:
		cCpuFreqManager(void);
		void Up(void);
		void Down(void);
		void Reset(void);

		bool SetCpuFreq(unsigned long CpuFreq);
		bool SetDelta(unsigned long Delta);
		unsigned long GetCpuFreq(void);
		unsigned long GetDelta(void);
};

class cPowerManager
{
	public:
		cPowerManager(void);
		virtual ~cPowerManager();

		bool Open(void);
		void Close(void);

		bool SetStandby(bool Active, bool Passive);
};

#endif // __PWRMNGR_H__
