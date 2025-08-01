/*---------------------------------------------------------------------------------

	default ARM7 core

		Copyright (C) 2005 - 2010
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.

	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.

	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <string.h>
#include "fpsAdjust.h"

static fpsa_t sActiveFpsa;

extern void enableSound();

//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------
	inputGetAndSend();
}

static void vcountIrqLower()
{
    while (1)
    {
        if (sActiveFpsa.initial)
        {
            sActiveFpsa.initial = FALSE;
            break;
        }

        if (!sActiveFpsa.backJump)
            sActiveFpsa.cycleDelta += sActiveFpsa.targetCycles - ((u64)FPSA_CYCLES_PER_FRAME << 24);
        u32 linesToAdd = 0;
        while (sActiveFpsa.cycleDelta >= (s64)((u64)FPSA_CYCLES_PER_LINE << 23))
        {
            sActiveFpsa.cycleDelta -= (u64)FPSA_CYCLES_PER_LINE << 24;
            if (++linesToAdd == 5)
                break;
        }
        if (linesToAdd == 0)
        {
            sActiveFpsa.backJump = FALSE;
            break;
        }
        if (linesToAdd > 1)
        {
            sActiveFpsa.backJump = TRUE;
        }
        else
        {
            // don't set the backJump flag because the irq is not retriggered if the new vcount
            // is the same as the previous line
            sActiveFpsa.backJump = FALSE;
        }
        // ensure we won't accidentally run out of line time
        while (REG_DISPSTAT & DISP_IN_HBLANK)
            ;
        int curVCount = REG_VCOUNT;
        REG_VCOUNT = curVCount - (linesToAdd - 1);
        if (linesToAdd == 1)
            break;

        while (REG_VCOUNT >= curVCount)//FPSA_ADJUST_MAX_VCOUNT - 5)
            ;
        while (REG_VCOUNT < curVCount)//FPSA_ADJUST_MAX_VCOUNT - 5)
            ;
    }
    REG_IF = IRQ_VCOUNT;
}

static void vcountIrqHigher()
{
    if (sActiveFpsa.initial)
    {
        sActiveFpsa.initial = FALSE;
        return;
    }
    sActiveFpsa.cycleDelta += ((u64)FPSA_CYCLES_PER_FRAME << 24) - sActiveFpsa.targetCycles;
    u32 linesToSkip = 0;
    while (sActiveFpsa.cycleDelta >= (s64)((u64)FPSA_CYCLES_PER_LINE << 23))
    {
        sActiveFpsa.cycleDelta -= (u64)FPSA_CYCLES_PER_LINE << 24;
        if (++linesToSkip == 5)
            break;
    }
    if (linesToSkip == 0)
        return;
    // ensure we won't accidentally run out of line time
    while (REG_DISPSTAT & DISP_IN_HBLANK)
        ;
    REG_VCOUNT = REG_VCOUNT + (linesToSkip + 1);
}

void fpsa_init(fpsa_t* fpsa)
{
    memset(fpsa, 0, sizeof(fpsa_t));
    fpsa->isStarted = FALSE;
    fpsa_setTargetFrameCycles(fpsa, (u64)FPSA_CYCLES_PER_FRAME << 24); // default to no adjustment
}

void fpsa_start(fpsa_t* fpsa)
{
    int irq = enterCriticalSection();
    do
    {
        if (fpsa->isStarted)
            break;
        if (fpsa->targetCycles == ((u64)FPSA_CYCLES_PER_FRAME << 24))
            break;
        irqDisable(IRQ_VCOUNT);
        fpsa->backJump = FALSE;
        fpsa->cycleDelta = 0;
        fpsa->initial = TRUE;
        fpsa->isFpsLower = fpsa->targetCycles >= ((u64)FPSA_CYCLES_PER_FRAME << 24);
        // prevent the irq from immediately happening
        while (REG_VCOUNT != FPSA_ADJUST_MAX_VCOUNT + 2)
            ;
        fpsa->isStarted = TRUE;
        if (fpsa->isFpsLower)
        {
            SetYtrigger(FPSA_ADJUST_MAX_VCOUNT - 5);
            irqSet(IRQ_VCOUNT, vcountIrqLower);
        }
        else
        {
            SetYtrigger(FPSA_ADJUST_MIN_VCOUNT);
            irqSet(IRQ_VCOUNT, vcountIrqHigher);
        }
        irqEnable(IRQ_VCOUNT);
    } while (0);
    leaveCriticalSection(irq);
}

void fpsa_stop(fpsa_t* fpsa)
{
    if (!fpsa->isStarted)
        return;
    fpsa->isStarted = FALSE;
    irqDisable(IRQ_VCOUNT);
}

void fpsa_setTargetFrameCycles(fpsa_t* fpsa, u64 cycles)
{
    fpsa->targetCycles = cycles;
}

void fpsa_setTargetFpsFraction(fpsa_t* fpsa, u32 num, u32 den)
{
    u64 cycles = (((double)FPSA_SYS_CLOCK * den * (1 << 24)) / num) + 0.5;
    fpsa_setTargetFrameCycles(fpsa, cycles);//((((u64)FPSA_SYS_CLOCK * (u64)den) << 24) + ((num + 1) >> 1)) / num);
}

void IPCSyncHandler(void) {
	bool startFpsa = false;
	u32 num = 0;
	u32 den = 0;
	switch (IPC_GetSync()) {
		case 0:
		default:
			fpsa_stop(&sActiveFpsa);
			break;
		case 1:
			// 47.95 FPS
			num = 48;
			den = 1;
			startFpsa = true;
			break;
		case 2: {
			// 50 FPS
			num = 44000;
			den = 1001;
			startFpsa = true;
		}	break;
		case 3:
			// 59.94 FPS
			num = 60;
			den = 1;
			startFpsa = true;
			break;
	}

	if (startFpsa) {
		int vblankCount = 1;
		while (num * (vblankCount + 1) / den < 62)
			vblankCount++;

		// safety
		if (num * vblankCount / den < 62)
		{
			fpsa_init(&sActiveFpsa);
			fpsa_setTargetFpsFraction(&sActiveFpsa, num * vblankCount, den);
			fpsa_start(&sActiveFpsa);
		}
	}
}

volatile bool exitflag = false;

//---------------------------------------------------------------------------------
void powerButtonCB() {
//---------------------------------------------------------------------------------
	exitflag = true;
}

//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------
    // nocashMessage("ARM7 main.c main");
	
	// Grab from DS header in GBA slot
	// *(u16*)0x02FFFC36 = *(u16*)0x0800015E;	// Header CRC16
	// *(u32*)0x02FFFC38 = *(u32*)0x0800000C;	// Game Code

	// clear sound registers
	dmaFillWords(0, (void*)0x04000400, 0x100);

	enableSound();

	readUserSettings();
	ledBlink(0);

	irqInit();
	// Start the RTC tracking IRQ
	initClockIRQ();
	
	touchInit();
	fifoInit();
	
	SetYtrigger(80);
	
	installSoundFIFO();
	installSystemFIFO();

	irqSet(IRQ_VBLANK, VblankHandler);
	irqSet(IRQ_IPC_SYNC, IPCSyncHandler);
	irqEnable(IRQ_VBLANK | IRQ_IPC_SYNC);

	setPowerButtonCB(powerButtonCB);
	
	// Keep the ARM7 mostly idle
	while (!exitflag) {
		if ( 0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R))) {
			exitflag = true;
		}
		// fifocheck();
		swiWaitForVBlank();
	}
	return 0;
}

