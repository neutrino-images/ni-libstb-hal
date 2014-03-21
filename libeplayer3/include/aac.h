/*
 * aac helper
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

#ifndef acc_123
#define acc_123

#define AAC_HEADER_LENGTH       7

static inline int aac_get_sample_rate_index(uint32_t sample_rate)
{
    if (96000 <= sample_rate)
	return 0;
    if (88200 <= sample_rate)
	return 1;
    if (64000 <= sample_rate)
	return 2;
    if (48000 <= sample_rate)
	return 3;
    if (44100 <= sample_rate)
	return 4;
    if (32000 <= sample_rate)
	return 5;
    if (24000 <= sample_rate)
	return 6;
    if (22050 <= sample_rate)
	return 7;
    if (16000 <= sample_rate)
	return 8;
    if (12000 <= sample_rate)
	return 9;
    if (11025 <= sample_rate)
	return 10;
    if (8000 <= sample_rate)
	return 11;
    if (7350 <= sample_rate)
	return 12;
    return 13;
}

#endif
