/******************************************************************************
 *******************************************************************************
	A simple font class for Easy GL2D DS created by:

	Relminator (Richard Eric M. Lope BSN RN)
	Http://Rel.Phatcode.Net

 *******************************************************************************
 ******************************************************************************/

#include <nds.h>
#include <stdio.h>
#include "large_font.h"
#include "uvcoord_large_font.h"
#include "FontGraphic.h"


/**
 * Get the index in the UV coordinate array where the letter appears
 */
unsigned int FontGraphic::getSpriteIndex(const u16 letter) {
	unsigned int spriteIndex = 0;
	for (unsigned int i = 0; i < LARGE_FONT_NUM_IMAGES; i++) {
		if (large_utf16_lookup_table[i] == letter) {
			spriteIndex = i;
			break;
		}
	}
	return spriteIndex;
}

void FontGraphic::printFontChar(int x, int y, unsigned short int fontChar)
{
	const u8* font = (u8*)large_fontBitmap;
	const int width = large_font_texcoords[2 + (fontChar * 4)];
	const int height = large_font_texcoords[3 + (fontChar * 4)];

	for (int h = 0; h < height; h++) {
		if ((y + h) >= 192) break;
		for (int w = 0; w < width; w += 2) {
			int c = 0;
			u8 fontPixel = font[((large_font_texcoords[1 + (fontChar * 4)] + h) * 256) + (large_font_texcoords[fontChar * 4] / 2) + (w / 2)];
			while (fontPixel >= 0x10) {
				fontPixel -= 0x10;
				c++;
			}

			if ((x + w + 1) < 256 && (w + 1) <= width) {
				if (c != 0) BG_GFX[((y + h) * 256) + (x + w + 1)] = large_fontPal[c] | BIT(15);
			}

			c = 0;
			while (fontPixel != 0) {
				fontPixel--;
				c++;
			}

			if ((x + w) < 256) {
				if (c != 0) BG_GFX[((y + h) * 256) + (x + w)] = large_fontPal[c] | BIT(15);
			}
		}
	}
}

void FontGraphic::print(int x, int y, const char *text)
{
	unsigned short int fontChar;
	unsigned char lowBits;
	unsigned char highBits;
	while (*text)
	{
		lowBits = *(unsigned char*) text++;
		if (lowBits != UTF16_SIGNAL_BYTE) { // check if the lower bits is the signal bits.
			fontChar = getSpriteIndex(lowBits);
		} else {
			lowBits = *(unsigned char*) text++; // LSB
			highBits = *(unsigned char*) text++; // HSB
			fontChar = getSpriteIndex((u16)(lowBits | highBits << 8));
		}
		
		printFontChar(x, y, fontChar);
		x += large_font_texcoords[2 + (fontChar * 4)];
	}
}

int FontGraphic::calcWidth(const char *text)
{
	unsigned short int fontChar;
	unsigned char lowBits;
	unsigned char highBits;
	int x = 0;

	while (*text)
	{
		lowBits = *(unsigned char*) text++;
		if (lowBits != UTF16_SIGNAL_BYTE) {
			fontChar = getSpriteIndex(lowBits);
		} else {
			lowBits = *(unsigned char*) text++;
			highBits = *(unsigned char*) text++;
			fontChar = getSpriteIndex((u16)(lowBits | highBits << 8));
		}

		x += large_font_texcoords[2 + (fontChar * 4)];
	}
	return x;
}

int FontGraphic::getCenteredX(const char *text)
{
	unsigned short int fontChar;
	unsigned char lowBits;
	unsigned char highBits;
	int total_width = 0;
	while (*text)
	{
		lowBits = *(unsigned char*) text++;
		if (lowBits != UTF16_SIGNAL_BYTE) {
			fontChar = getSpriteIndex(lowBits);
		} else {
			lowBits = *(unsigned char*) text++;
			highBits = *(unsigned char*) text++;
			fontChar = getSpriteIndex((u16)(lowBits | highBits << 8));
		}

		total_width += large_font_texcoords[2 + (fontChar * 4)];
	}
	const int result = (SCREEN_WIDTH - total_width) / 2;
	return result;
}

void FontGraphic::printCentered(int y, const char *text)
{
	unsigned short int fontChar;
	unsigned char lowBits;
	unsigned char highBits;	

	int x = getCenteredX(text);
	while (*text)
	{
		lowBits = *(unsigned char*) text++;
		if (lowBits != UTF16_SIGNAL_BYTE) {
			fontChar = getSpriteIndex(lowBits);
		} else {
			lowBits = *(unsigned char*) text++;
			highBits = *(unsigned char*) text++;
			fontChar = getSpriteIndex((u16)(lowBits | highBits << 8));
		}

		printFontChar(x, y, fontChar);
		x += large_font_texcoords[2 + (fontChar * 4)];
	}
}
