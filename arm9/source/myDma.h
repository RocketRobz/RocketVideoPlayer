#include <dma.h>

inline void dmaFillHalfWordsAsynch(uint8 channel, u16 value, void* dest, uint32 size) {
	DMA_FILL(channel) = (uint32)value;
	DMA_SRC(channel) = (uint32)&DMA_FILL(channel);

	DMA_DEST(channel) = (uint32)dest;
	DMA_CR(channel) = DMA_SRC_FIX | DMA_COPY_HALFWORDS | (size>>1);
}
