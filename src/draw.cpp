#include <ps1/gpucmd.h>
#include <ps1/registers.h>

#include "draw.h"
#include "Sprite.h"
#include "GameObject.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#define CHAIN_BUFFER_SIZE 1024

typedef struct {
	uint32_t data[CHAIN_BUFFER_SIZE];
	uint32_t *nextPacket;
} DMAChain;


GameObject* obj;
Sprite* spr;


// Private util functions

static void gpu_gp0_wait_ready(void) {
	while (!(GPU_GP1 & GP1_STAT_CMD_READY))
		__asm__ volatile("");
}

static void dma_send_linked_list(const void *data) {

	// Wait until the GPU's DMA unit has finished sending data and is ready.
	while (DMA_CHCR(DMA_GPU) & DMA_CHCR_ENABLE)
		__asm__ volatile("");

	// Make sure the pointer is aligned to 32 bits (4 bytes). The DMA engine is
	// not capable of reading unaligned data.
	//assert(!((uint32_t) data % 4));

	// Give DMA a pointer to the beginning of the data and tell it to send it in
	// linked list mode. The DMA unit will start parsing a chain of "packets"
	// from RAM, with each packet being made up of a 32-bit header followed by
	// zero or more 32-bit commands to be sent to the GP0 register.
	DMA_MADR(DMA_GPU) = (uint32_t) data;
	DMA_CHCR(DMA_GPU) = DMA_CHCR_WRITE | DMA_CHCR_MODE_LIST | DMA_CHCR_ENABLE;
}

static uint32_t *dma_allocate_packet(DMAChain *chain, int numCommands) {
	// Grab the current pointer to the next packet then increment it to allocate
	// a new packet. We have to allocate an extra word for the packet's header,
	// which will contain the number of GP0 commands the packet is made up of as
	// well as a pointer to the next packet (or a special "terminator" value to
	// tell the DMA unit to stop).
	uint32_t *ptr      = chain->nextPacket;
	chain->nextPacket += numCommands + 1;

	// Write the header and set its pointer to point to the next packet that
	// will be allocated in the buffer.
	*ptr = gp0_tag(numCommands, chain->nextPacket);

	// Make sure we haven't yet run out of space for future packets or a linked
	// list terminator, then return a pointer to the packet's first GP0 command.
	//assert(chain->nextPacket < &(chain->data)[CHAIN_BUFFER_SIZE]);

	return &ptr[1];
}


void gpu_setup(GP1VideoMode mode, int width, int height) {

	DMA_DPCR |= DMA_DPCR_ENABLE << (DMA_GPU * 4);

	obj = new GameObject(100,100,100);
	spr = new Sprite(SPRITE_TYPE_FLAT_COLOR);

	spr->Color.x = 0;
	spr->Color.y = 255;
	spr->Color.z = 0;

	obj->components[0] = spr;

    // Origin of framebuffer based on if PAL or NTSC
    int x = 0x760;
    int y = (mode = GP1_MODE_PAL) ? 0xa3 : 0x88;

    // We need to do some timing magic to actually achieve our desired resolution
	GP1HorizontalRes horizontalRes = GP1_HRES_320;
	GP1VerticalRes   verticalRes   = GP1_VRES_256;

    int offsetX = (width  * gp1_clockMultiplierH(horizontalRes)) / 2;
	int offsetY = (height / gp1_clockDividerV(verticalRes))      / 2;

    GPU_GP1 = gp1_resetGPU();
	GPU_GP1 = gp1_fbRangeH(x - offsetX, x + offsetX);
	GPU_GP1 = gp1_fbRangeV(y - offsetY, y + offsetY);
	GPU_GP1 = gp1_fbMode(
		horizontalRes, verticalRes, mode, false, GP1_COLOR_16BPP
	);

}

static void gpu_wait_vsync(void) {
	while (!(IRQ_STAT & (1 << IRQ_VSYNC)))
		__asm__ volatile("");

	IRQ_STAT = ~(1 << IRQ_VSYNC);
}

// Engine's API

void draw_init() {
    if ((GPU_GP1 & GP1_STAT_MODE_BITMASK) == GP1_STAT_MODE_PAL) {
		gpu_setup(GP1_MODE_PAL, SCREEN_WIDTH, SCREEN_HEIGHT);
	} else {
		gpu_setup(GP1_MODE_NTSC, SCREEN_WIDTH, SCREEN_HEIGHT);
	}

	GPU_GP1 = gp1_dispBlank(false);
}

int goRight = 1;

bool currentBuffer = false;
DMAChain dmaChains[2];
void draw_update() {
	int frameX = currentBuffer ? SCREEN_WIDTH : 0;
	int frameY = 0;

	DMAChain *chain  = &dmaChains[currentBuffer];
	currentBuffer = !currentBuffer;

	uint32_t *ptr;
	GPU_GP1 = gp1_fbOffset(frameX, frameY);

	chain->nextPacket = chain->data;

	ptr = dma_allocate_packet(chain, 4);
	ptr[0] = gp0_texpage(0, true, false);
	ptr[1] = GPU_GP0 = gp0_fbOffset1(frameX, frameY);
	ptr[2] = GPU_GP0 = gp0_fbOffset2(
		frameX + SCREEN_WIDTH - 1, frameY + SCREEN_HEIGHT - 2
	);
	ptr[3] = gp0_fbOrigin(frameX, frameY);

	ptr = dma_allocate_packet(chain, 3);
	ptr[0] = gp0_rgb(64, 64, 64) | gp0_vramFill();
	ptr[1] = gp0_xy(frameX, frameY);
    ptr[2] = gp0_xy(SCREEN_WIDTH, SCREEN_HEIGHT);

	ptr = dma_allocate_packet(chain, 3);
	obj->execute(ptr);

	if(obj->position.x+spr->Width >= SCREEN_WIDTH && goRight) {
		goRight = 0;
	}

	if(!goRight && obj->position.x <= 0) {
		goRight = 1;
	}

	if(goRight) {
		obj->position.x++;
	}
	else {
		obj->position.x--;
	}

	*(chain->nextPacket) = gp0_endTag(0);
	gpu_gp0_wait_ready();
	gpu_wait_vsync();
	dma_send_linked_list(chain->data);
}


