#include <nds.h>

#define __itcm __attribute__((section(".itcm")))

void __itcm
lzssDecompress(byte source[], byte destination[]) {
	uint32 leng = (uint)(source[1] | (source[2] << 8) | (source[3] << 16));
	int Offs = 4;
	int dstoffs = 0;
	while (true)
	{
		byte header = source[Offs++];
		for (int i = 0; i < 8; i++)
		{
			if ((header & 0x80) == 0) destination[dstoffs++] = source[Offs++];
			else
			{
				byte a = source[Offs++];
				byte b = source[Offs++];
				int offs = (((a & 0xF) << 8) | b) + 1;
				int length = (a >> 4) + 3;
				for (int j = 0; j < length; j++)
				{
					destination[dstoffs] = destination[dstoffs - offs];
					dstoffs++;
				}
			}
			if (dstoffs >= leng) break;
			header <<= 1;
		}
	}
}
