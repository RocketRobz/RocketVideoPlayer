/*-----------------------------------------------------------------
 Copyright (C) 2015
	Matthew Scholefield

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/

#include <list>
#include <stdio.h>
#include <nds/interrupts.h>
#include "FontGraphic.h"
#include "fontHandler.h"
#include <nds.h>

#include "small_font.h"
#include "large_font.h"
#include "../userpal.h"

extern u16* colorTable;

void fontInit(const int favoriteColor)
{
	u16* largePalChange = (u16*)large_fontPal;
	largePalChange[1] = userPal[favoriteColor][1];
	largePalChange[2] = userPal[favoriteColor][2];

	if (colorTable) {
		u16* smallPalChange = (u16*)small_fontPal;
		for (int i = 0; i < 5; i++) {
			smallPalChange[i] = colorTable[smallPalChange[i]];
		}

		for (int i = 0; i < 4; i++) {
			largePalChange[i] = colorTable[largePalChange[i]];
		}
	}
}

void printLarge(int x, int y, const char *message)
{
	FontGraphic().print(x, y, message);
}

void printLargeCentered(int y, const char *message)
{
	FontGraphic().printCentered(y, message);
}

int calcLargeFontWidth(const char *text)
{
	return FontGraphic().calcWidth(text);
}
