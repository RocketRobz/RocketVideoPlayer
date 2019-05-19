#include <nds.h>

#include "gl2d.h"

typedef struct rvidHeaderInfo {
	u32 formatString;	// "RVID" string
	u32 ver;			// File format version
	u32 frames;			// Number of frames
	u8 fps;				// Frames per second
	u8 vRes;			// Vertical resolution
} rvidHeaderInfo;

extern rvidHeaderInfo rvidHeader;
extern int currentFrame;

void renderGui(void) {
	glBegin2D();
	{
		glBoxFilled(0, 0, 255, 191, RGB15(255/8, 255/8, 255/8));		// BG
		glBoxFilled(0, 0, 255, 59, RGB15(0/8, 176/8, 248/8));			// Title bar
		glBoxFilled(0, 58, 255, 58, RGB15(160/8, 224/8, 248/8));		// Title bar edge
		glBoxFilled(0, 160, 255, 191, RGB15(184/8, 184/8, 184/8));	// Bottom gray bar
		glBoxFilled(14, 142, 241, 145, RGB15(152/8, 152/8, 152/8));	// Play bar horizontal edge
		glBoxFilled(15, 141, 240, 146, RGB15(152/8, 152/8, 152/8));	// Play bar mid edge
		glBoxFilled(16, 140, 239, 147, RGB15(152/8, 152/8, 152/8));	// Play bar vertical edge
		glBoxFilled(16, 142, 239, 145, RGB15(255/8, 255/8, 255/8));	// Behind gray part of play bar
		/*
		glBoxFilled(4+(currentFrame/(rvidHeader.frames/184)), 142, 247, 145, RGB15(96/8, 96/8, 96/8));	// Gray part of play bar
		glBoxFilled(12+(currentFrame/(rvidHeader.frames/184)), 134, 19+(currentFrame/(rvidHeader.frames/184)), 153, RGB15(120/8, 120/8, 120/8));	// Play bar slider edge
		glBoxFilled(14+(currentFrame/(rvidHeader.frames/184)), 136, 17+(currentFrame/(rvidHeader.frames/184)), 151, RGB15(255/8, 255/8, 255/8));	// Play bar slider
		glBoxFilled(14+(currentFrame/(rvidHeader.frames/184)), 148, 17+(currentFrame/(rvidHeader.frames/184)), 151, RGB15(216/8, 216/8, 216/8));	// Play bar slider shading
		*/
		glColor(RGB15(31, 31, 31));
	}
	glEnd2D();
	GFX_FLUSH = 0;
}
