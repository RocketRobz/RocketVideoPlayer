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

vu32* sharedAddr = (vu32*)0x02FFFD00;

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
        if (++linesToSkip == sActiveFpsa.linesToSkipMax)
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
	fpsa->linesToSkipMax = (num / den > 62) ? 55 : 5;
}

void IPCSyncHandler(void) {
	bool startFpsa = false;
	switch (IPC_GetSync()) {
		case 0:
		default:
			fpsa_stop(&sActiveFpsa);
			break;
		case 1:
			startFpsa = true;
			break;
	}

	if (startFpsa) {
		const u32 num = sharedAddr[0];
		const u32 den = sharedAddr[1];
		const int max = (num / den > 62) ? 74 : 62;

		int vblankCount = 1;
		while (num * (vblankCount + 1) / den < max)
			vblankCount++;

		// safety
		if (num * vblankCount / den < max)
		{
			fpsa_init(&sActiveFpsa);
			fpsa_setTargetFpsFraction(&sActiveFpsa, num * vblankCount, den);
			fpsa_start(&sActiveFpsa);
		}
	}
}

void playRvidAudio(u32 value, void* userdata) {
	const u16 freq = sharedAddr[3];
	for (int channel = 0; channel < 2; channel++) {
		SCHANNEL_CR(channel) &= ~SCHANNEL_ENABLE;
		SCHANNEL_SOURCE(channel) = sharedAddr[channel];
		SCHANNEL_LENGTH(channel) = sharedAddr[2];
		SCHANNEL_TIMER(channel) = SOUND_FREQ(freq);
		SCHANNEL_CR(channel) = SCHANNEL_ENABLE | SOUND_VOL(127) | SOUND_PAN(channel ? 127 : 0) | (sharedAddr[4] ? SOUND_FORMAT_16BIT : SOUND_FORMAT_8BIT) | SOUND_ONE_SHOT;
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
    // Initialize sound hardware
    enableSound();

    // Read user information from the firmware (name, birthday, etc)
    readUserSettings();

    // Stop LED blinking
    ledBlink(LED_ALWAYS_ON);

    // Using the calibration values read from the firmware with
    // readUserSettings(), calculate some internal values to convert raw
    // coordinates into screen coordinates.
    touchInit();

    irqInit();
    fifoInit();

    installSoundFIFO();
    installSystemFIFO(); // Sleep mode, storage, firmware...

    // This sets a callback that is called when the power button in a DSi
    // console is pressed. It has no effect in a DS.
    setPowerButtonCB(powerButtonCB);

    // Read current date from the RTC and setup an interrupt to update the time
    // regularly. The interrupt simply adds one second every time, it doesn't
    // read the date. Reading the RTC is very slow, so it's a bad idea to do it
    // frequently.
    initClockIRQTimer(3);

    // Now that the FIFO is setup we can start sending input data to the ARM9.
	irqSet(IRQ_VBLANK, VblankHandler);
	irqSet(IRQ_IPC_SYNC, IPCSyncHandler);
	irqEnable(IRQ_VBLANK | IRQ_IPC_SYNC);

	// Check for 3DS in DSi mode, or DSi & 3DS in DS mode
	if (isDSiMode()) {
		const u8 i2cVer = i2cReadRegister(0x4A, 0);
		if (i2cVer == 0 || i2cVer == 0xFF) {
			fifoSendValue32(FIFO_USER_01, 0xD2); // If I2C is bricked, this is a DSi
		} else {
			const u8 byteBak = i2cReadRegister(0x4A, 0x71);
			i2cWriteRegister(0x4A, 0x71, 0xD2);
			fifoSendValue32(FIFO_USER_01, i2cReadRegister(0x4A, 0x71)); // If I2C write is successful, this is a DSi
			i2cWriteRegister(0x4A, 0x71, byteBak);
		}
	} else {
		// There is no (known) way to specifically check for 3DS in DS mode, so DSi will be affected as well
		fifoSendValue32(FIFO_USER_01, REG_SNDEXTCNT ? 0 : 0xD2);
	}
	{
		// Check if only bottom screen is turned on
		const u8 reg = readPowerManagement(PM_CONTROL_REG);
		if (!(reg & PM_BACKLIGHT_TOP) && (reg & PM_BACKLIGHT_BOTTOM)) {
			fifoSendValue32(FIFO_USER_02, 1);
		}
	}

	fifoSetValue32Handler(FIFO_USER_03, playRvidAudio, NULL);

	while (!exitflag) {
        const uint16_t key_mask = KEY_SELECT | KEY_START | KEY_L | KEY_R;
        uint16_t keys_pressed = ~REG_KEYINPUT;

        if ((keys_pressed & key_mask) == key_mask)
            exitflag = true;

		swiWaitForVBlank();
	}
	return 0;
}

