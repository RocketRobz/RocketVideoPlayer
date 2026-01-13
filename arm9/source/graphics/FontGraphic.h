/******************************************************************************
 *******************************************************************************
	A simple font class for Easy GL2D DS created by:

	Relminator (Richard Eric M. Lope BSN RN)
	Http://Rel.Phatcode.Net

 *******************************************************************************
 ******************************************************************************/
#pragma once
#define FONT_SX 8
#define FONT_SY 10
#define UTF16_SIGNAL_BYTE  0x0F

class FontGraphic
{
private:
	unsigned int getSpriteIndex(const u16 letter);

public:

	FontGraphic() { };
	void printFontChar(int x, int y, unsigned short int fontChar);
	void print(int x, int y, const char *text);
	int calcWidth(const char *text);
	int getCenteredX(const char *text);
	void printCentered(int y, const char *text);
};