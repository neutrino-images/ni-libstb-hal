/*
 * FLV Bitstream/VLC Decoder
 *
 * Copyright (c) 2006 vixy project
 *
 * This file contains the code that based on FFmpeg (http://ffmpeg.mplayerhq.hu/)
 * See original copyright notice in /FFMPEG_CREDITS and /FFMPEG_IMPORTS
 *
 * This file is part of VIXY FLV Converter.
 *
 * 'VIXY FLV Converter' is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * 'VIXY FLV Converter' is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include "bitreader.h"
#include "flv.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const uint8 zig_zag_scan[64] =
{
	0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
	12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

static const VLCtab vlc_table_intra_MCBPC[] = //: table_size=72 table_allocated=128 bits=6
{
	{64, -3},
	{5, 6}, {6, 6}, {7, 6}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {1, 3}, {1, 3}, {1, 3}, {1, 3}, {1, 3},
	{1, 3}, {1, 3}, {1, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {3, 3},
	{3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {-1, 0}, {8, 3}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}
};

static const VLCtab vlc_table_inter_MCBPC[] = //table_size=198 table_allocated=256 bits=7
{
	{128, -6}, {192, -2}, {196, -1}, {7, 7}, {18, 7}, {17, 7}, {10, 7}, {9, 7}, {12, 6}, {12, 6}, {3, 6}, {3, 6},
	{4, 5}, {4, 5}, {4, 5}, {4, 5}, {2, 4}, {2, 4}, {2, 4}, {2, 4}, {2, 4}, {2, 4}, {2, 4}, {2, 4}, {1, 4}, {1, 4},
	{1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {16, 3}, {16, 3}, {16, 3}, {16, 3}, {16, 3}, {16, 3}, {16, 3},
	{16, 3}, {16, 3}, {16, 3}, {16, 3}, {16, 3}, {16, 3}, {16, 3}, {16, 3}, {16, 3}, {8, 3}, {8, 3}, {8, 3}, {8, 3},
	{8, 3}, {8, 3}, {8, 3}, {8, 3}, {8, 3}, {8, 3}, {8, 3}, {8, 3}, {8, 3}, {8, 3}, {8, 3}, {8, 3}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0},
	{-1, 0}, {24, 4}, {24, 4}, {24, 4}, {24, 4}, {25, 6}, {-1, 0}, {26, 6}, {27, 6}, {20, 2}, {20, 2}, {20, 2},
	{20, 2}, {20, 2}, {20, 2}, {20, 2}, {20, 2}, {20, 2}, {20, 2}, {20, 2}, {20, 2}, {20, 2}, {20, 2}, {20, 2},
	{20, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2},
	{15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {14, 2}, {14, 2}, {14, 2}, {14, 2}, {14, 2}, {14, 2}, {14, 2},
	{14, 2}, {14, 2}, {14, 2}, {14, 2}, {14, 2}, {14, 2}, {14, 2}, {14, 2}, {14, 2}, {13, 2}, {11, 2}, {6, 1},
	{6, 1}, {5, 1}, {19, 1},
};

static const VLCtab vlc_table_cbpy[] = //table_size=64 table_allocated=64 bits=6
{
	{-1, 0}, {-1, 0}, {6, 6}, {9, 6}, {8, 5}, {8, 5}, {4, 5}, {4, 5}, {2, 5}, {2, 5}, {1, 5}, {1, 5}, {0, 4}, {0, 4},
	{0, 4}, {0, 4}, {12, 4}, {12, 4}, {12, 4}, {12, 4}, {10, 4}, {10, 4}, {10, 4}, {10, 4}, {14, 4}, {14, 4},
	{14, 4}, {14, 4}, {5, 4}, {5, 4}, {5, 4}, {5, 4}, {13, 4}, {13, 4}, {13, 4}, {13, 4}, {3, 4}, {3, 4}, {3, 4},
	{3, 4}, {11, 4}, {11, 4}, {11, 4}, {11, 4}, {7, 4}, {7, 4}, {7, 4}, {7, 4}, {15, 2}, {15, 2}, {15, 2}, {15, 2},
	{15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}, {15, 2}
};

static const VLCtab vlc_table_rl_inter[] = //: table_size=554 table_allocated=1024 bits=9
{
	{-1, 0}, {512, -2}, {516, -1}, {518, -1}, {520, -1}, {522, -1}, {524, -1}, {526, -1}, {528, -2}, {532, -2},
	{536, -3}, {544, -3}, {102, 7}, {102, 7}, {102, 7}, {102, 7}, {552, -1}, {85, 9}, {84, 9}, {83, 9}, {82, 9},
	{81, 9}, {80, 9}, {79, 9}, {78, 9}, {59, 9}, {53, 9}, {52, 9}, {51, 9}, {50, 9}, {49, 9}, {48, 9}, {47, 9},
	{46, 9}, {26, 9}, {23, 9}, {6, 9}, {5, 9}, {77, 8}, {77, 8}, {76, 8}, {76, 8}, {75, 8}, {75, 8}, {74, 8},
	{74, 8}, {73, 8}, {73, 8}, {72, 8}, {72, 8}, {71, 8}, {71, 8}, {70, 8}, {70, 8}, {45, 8}, {45, 8}, {44, 8},
	{44, 8}, {19, 8}, {19, 8}, {14, 8}, {14, 8}, {4, 8}, {4, 8}, {69, 7}, {69, 7}, {69, 7}, {69, 7}, {68, 7},
	{68, 7}, {68, 7}, {68, 7}, {67, 7}, {67, 7}, {67, 7}, {67, 7}, {66, 7}, {66, 7}, {66, 7}, {66, 7}, {43, 7},
	{43, 7}, {43, 7}, {43, 7}, {42, 7}, {42, 7}, {42, 7}, {42, 7}, {40, 7}, {40, 7}, {40, 7}, {40, 7}, {3, 7},
	{3, 7}, {3, 7}, {3, 7}, {65, 6}, {65, 6}, {65, 6}, {65, 6}, {65, 6}, {65, 6}, {65, 6}, {65, 6}, {64, 6}, {64, 6},
	{64, 6}, {64, 6}, {64, 6}, {64, 6}, {64, 6}, {64, 6}, {63, 6}, {63, 6}, {63, 6}, {63, 6}, {63, 6}, {63, 6},
	{63, 6}, {63, 6}, {61, 6}, {61, 6}, {61, 6}, {61, 6}, {61, 6}, {61, 6}, {61, 6}, {61, 6}, {38, 6}, {38, 6},
	{38, 6}, {38, 6}, {38, 6}, {38, 6}, {38, 6}, {38, 6}, {36, 6}, {36, 6}, {36, 6}, {36, 6}, {36, 6}, {36, 6},
	{36, 6}, {36, 6}, {34, 6}, {34, 6}, {34, 6}, {34, 6}, {34, 6}, {34, 6}, {34, 6}, {34, 6}, {31, 6}, {31, 6},
	{31, 6}, {31, 6}, {31, 6}, {31, 6}, {31, 6}, {31, 6}, {13, 6}, {13, 6}, {13, 6}, {13, 6}, {13, 6}, {13, 6},
	{13, 6}, {13, 6}, {2, 6}, {2, 6}, {2, 6}, {2, 6}, {2, 6}, {2, 6}, {2, 6}, {2, 6}, {28, 5}, {28, 5}, {28, 5},
	{28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5},
	{28, 5}, {25, 5}, {25, 5}, {25, 5}, {25, 5}, {25, 5}, {25, 5}, {25, 5}, {25, 5}, {25, 5}, {25, 5}, {25, 5},
	{25, 5}, {25, 5}, {25, 5}, {25, 5}, {25, 5}, {22, 5}, {22, 5}, {22, 5}, {22, 5}, {22, 5}, {22, 5}, {22, 5},
	{22, 5}, {22, 5}, {22, 5}, {22, 5}, {22, 5}, {22, 5}, {22, 5}, {22, 5}, {22, 5}, {58, 4}, {58, 4}, {58, 4},
	{58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4},
	{58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4},
	{58, 4}, {58, 4}, {58, 4}, {58, 4}, {58, 4}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
	{0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
	{0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
	{0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
	{0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
	{0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
	{0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
	{0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
	{0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
	{0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3},
	{12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3},
	{12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3},
	{12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3},
	{12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3},
	{12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {12, 3}, {18, 4},
	{18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4},
	{18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4},
	{18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {18, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4},
	{1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4},
	{1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {1, 4}, {62, 2}, {60, 2},
	{10, 2}, {9, 2}, {89, 1}, {88, 1}, {87, 1}, {86, 1}, {39, 1}, {37, 1}, {35, 1}, {32, 1}, {29, 1}, {24, 1},
	{20, 1}, {15, 1}, {11, 2}, {16, 2}, {54, 2}, {55, 2}, {90, 2}, {91, 2}, {92, 2}, {93, 2}, {17, 3}, {21, 3},
	{27, 3}, {30, 3}, {33, 3}, {41, 3}, {56, 3}, {57, 3}, {94, 3}, {95, 3}, {96, 3}, {97, 3}, {98, 3}, {99, 3},
	{100, 3}, {101, 3}, {8, 1}, {7, 1}
};

static const VLCtab vlc_table_mv[] = //mv_vlc: table_size=538 table_allocated=1024 bits=9
{
	{512, -3}, {520, -2}, {524, -1}, {526, -1}, {528, -1}, {530, -1}, {532, -1}, {534, -1}, {536, -1}, {10, 9},
	{9, 9}, {8, 9}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {6, 7}, {6, 7}, {6, 7}, {6, 7}, {5, 7}, {5, 7}, {5, 7}, {5, 7},
	{4, 6}, {4, 6}, {4, 6}, {4, 6}, {4, 6}, {4, 6}, {4, 6}, {4, 6}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4},
	{3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4},
	{3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {2, 3}, {2, 3},
	{2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3},
	{2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3},
	{2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3},
	{2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3},
	{2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
	{1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
	{1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
	{1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
	{1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
	{1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
	{1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
	{1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
	{1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
	{1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1},
	{0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {-1, 0}, {-1, 0},
	{32, 3}, {31, 3}, {30, 2}, {30, 2}, {29, 2}, {29, 2}, {28, 2}, {27, 2}, {26, 2}, {25, 2}, {24, 1}, {23, 1}, {22, 1},
	{21, 1}, {20, 1}, {19, 1}, {18, 1}, {17, 1}, {16, 1}, {15, 1}, {14, 1}, {13, 1}, {12, 1}, {11, 1},
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline int decode_DC(BR *p)
{
	int level = get_bits(p, 8);
	if ((level & 0x7f) == 0)
	{
		printf("illigal dc\n");
		return -1;
	}

	if (level == 255)
		level = 128;

//	printf("DC: %d\n", level);
	return level;
}

static inline int decode_AC(BR *p, BLOCK *block, int escape_type, int i)
{
	int code, run, level, last, sign;

	while (1)
	{
		code = get_vlc(p, vlc_table_rl_inter, 9, 2);
		if (code < 0)
		{
			printf("invalid Huffman code in getblock()\n");
			return -1;
		}

		if (code == rl_inter_n)
		{
			//escape
			if (escape_type == 1)
			{
				int is11bit = get_bits(p, 1);

				last = get_bits(p, 1);
				run = get_bits(p, 6);
				if (is11bit)
				{
					level = get_sbits(p, 11);
				}
				else
				{
					level = get_sbits(p, 7);
				}
			}
			else
			{
				last = get_bits(p, 1);
				run = get_bits(p, 6);
				level = get_bits(p, 8);
				sign = level >= 128;

				if (sign)
					level = 256 - level;
			}
		}
		else
		{
			run = rl_inter_run[code];
			level = rl_inter_level[code];
			last = code >= rl_inter_last;

			sign = get_bits(p, 1);

			if (sign)
				level = -level;
		}

		i += run;
		if (i >= 64)
		{
			printf("run overflow..\n");
			return -1;
		}
		block->block[zig_zag_scan[i]] = level;

		if (last)
			break;
		i++;
	}

	block->last_index = i;
	return 0;
}

static inline int decode_intra_block(BR *p, BLOCK *block, int escape_type, int coded)
{
	int level = decode_DC(p);
	if (level < 0)
	{
		printf("dc error.\n");
		return -1;
	}

	block->block[0] = level;
	block->last_index = 0;

	if (!coded)
	{
		return 0;
	}

	if (decode_AC(p, block, escape_type, 1) < 0)
		return -1;

	return 0;
}

static inline int decode_inter_block(BR *p, BLOCK *block, int escape_type, int coded)
{
	block->last_index = -1;

	if (!coded)
	{
		return 0;
	}

	if (decode_AC(p, block, escape_type, 0) < 0)
		return -1;

	return 0;
}

static inline int get_intra_MCBPC(BR *br)
{
	int cbpc;
	do
	{
		cbpc = get_vlc(br, vlc_table_intra_MCBPC, 6, 2);
		if (cbpc < 0)
			return -1;
	}
	while (cbpc == 8);
	return cbpc;
}

static inline int get_inter_MCBPC(BR *br)
{
	int cbpc;
	do
	{
		if (get_bits(br, 1))
		{
			return -2;
		}
		cbpc = get_vlc(br, vlc_table_inter_MCBPC, 7, 2);
		if (cbpc < 0)
			return -1;
	}
	while (cbpc == 20);

	return cbpc;
}

static inline int get_cbpy(BR *br)
{
	return get_vlc(br, vlc_table_cbpy, 6, 1);
}

static inline int decode_dquant(BR *p)
{
	static const int table[4] = { -1, -2, 1, 2 };
	return table[get_bits(p, 2)];
}

static inline int decode_motion(BR *br, VLCDEC *vlcdec)
{
	int tmp;
	int code = get_vlcdec(br, vlc_table_mv, 9, 2, vlcdec);

	vlcdec->bits_ex = 0;

	if (code == 0)
		return 0;
	if (code < 0)
		return -1;

	tmp = get_bits(br, 1);
	/*
	    vlcdec->value |= (tmp << vlcdec->bits);
	/// vlcdec->value <<= 1;
	//  vlcdec->value |= tmp;
	    vlcdec->bits++;
	//  vlcdec->value |= (tmp << vlcdec->bits);
	*/
	vlcdec->bits_ex = 1;
	vlcdec->value_ex = tmp;

	return 0;
}

static inline int decode_intra_mb_internal(BR *p, MICROBLOCK *mb, int escape_type, int qscale, int cbpc, int dquant)
{
	int cbpy, cbp;
	int i;

	cbpy = get_cbpy(p);
	if (cbpy < 0)
	{
		printf("cbpy error\n");
		return -1;
	}

	cbp = (cbpc & 3) | (cbpy << 2);

	if (dquant)
	{
		mb->dquant = decode_dquant(p);
		qscale += mb->dquant;
	}

	for (i = 0; i < 6; i++)
	{
		if (decode_intra_block(p, &mb->block[i], escape_type, cbp & 32) != 0)
			return -1;
		cbp += cbp;
	}

	return 0;
}

static inline int decode_inter_mb_internal(BR *p, MICROBLOCK *mb, int escape_type, int qscale, int cbpc, int dquant)
{
	int cbpy, cbp;
	int i;

	cbpy = get_cbpy(p);
	if (cbpy < 0)
	{
		printf("cbpy error\n");
		return -1;
	}

	cbpy ^= 0xF;
	cbp = (cbpc & 3) | (cbpy << 2);

	if (dquant)
	{
		mb->dquant = decode_dquant(p);
		qscale += mb->dquant;
	}

	if ((cbpc & 16) == 0)
	{
		// 16x16 motion prediction
		decode_motion(p, &mb->mv_x[0]);
		decode_motion(p, &mb->mv_y[0]);
		mb->mv_type = MV_TYPE_16X16;
	}
	else
	{
		// 8x8 motion prediction
		for (i = 0; i < 4; i++)
		{
			decode_motion(p, &mb->mv_x[i]);
			decode_motion(p, &mb->mv_y[i]);
		}
		mb->mv_type = MV_TYPE_8X8;
	}

	for (i = 0; i < 6; i++)
	{
		if (decode_inter_block(p, &mb->block[i], escape_type, cbp & 32) != 0)
			return -1;
		cbp += cbp;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int decode_I_mb(BR *p, MICROBLOCK *mb, int escape_type, int qscale)
{
	int dquant;
	int cbpc = get_intra_MCBPC(p);
	if (cbpc < 0)
	{
		printf("intra_MCBPC error\n");
		return -1;
	}

	dquant = cbpc & 4;
	decode_intra_mb_internal(p, mb, escape_type, qscale, cbpc, dquant);

	if (show_bits(p, 16) == 0)
	{
//		printf("slice end???\n");
	}

	return 0;
}

int decode_P_mb(BR *p, MICROBLOCK *mb, int escape_type, int qscale)
{
	int cbpc = get_inter_MCBPC(p);

	if (cbpc == -1)
	{
		printf("inter_MCBPC error\n");
		return -1;
	}
	if (cbpc == -2)
	{
		mb->skip = 1;
	}
	else if (cbpc >= 0)
	{
		int dquant = cbpc & 8;
		mb->skip = 0;

		if ((cbpc & 4) != 0)
		{
			mb->intra = 1;
			decode_intra_mb_internal(p, mb, escape_type, qscale, cbpc, dquant);
		}
		else
		{
			mb->intra = 0;
			decode_inter_mb_internal(p, mb, escape_type, qscale, cbpc, dquant);
		}
	}

	if (show_bits(p, 16) == 0)
	{
//		printf("slice end???\n");
	}

	return 0;
}

int decode_picture_header(BR *p, PICTURE *picture)
{
	int tmp, width = 0, height = 0;

	if (get_bits(p, 17) != 1)
	{
		return -1;
	}

	tmp = get_bits(p, 5);
	if (tmp != 0 && tmp != 1)
	{
		return -1;
	}

	picture->escape_type = tmp;
	picture->frame_number = get_bits(p, 8);

	tmp = get_bits(p, 3);
	switch (tmp)
	{
		case 0:
			width = get_bits(p, 8);
			height = get_bits(p, 8);
			break;
		case 1:
			width = get_bits(p, 16);
			height = get_bits(p, 16);
			break;
		case 2:
			width = 352, height = 288;
			break;
		case 3:
			width = 176, height = 144;
			break;
		case 4:
			width = 128, height = 96;
			break;
		case 5:
			width = 320, height = 240;
			break;
		case 6:
			width = 160, height = 120;
			break;
		default:
			return -1;
	}

	picture->width = width;
	picture->height = height;
//	printf("width: %d height: %d\n", width, height);

	picture->picture_type = get_bits(p, 2);
//	printf("picture_type: %d\n", tmp);

	tmp = get_bits(p, 1);
//	printf("deblocking flag: %d\n", tmp);

	tmp = get_bits(p, 5);
	picture->qscale = tmp;
//	printf("qscale: %d\n", tmp);

	// PEI
	while (get_bits(p, 1) != 0)
	{
		flash_bits(p, 8);
	}

//	dump_marker(0, "dd header end");
	return 0;
}
