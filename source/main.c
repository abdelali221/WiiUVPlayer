#include <stdio.h>
#include <stdlib.h>
#include <ogc/machine/processor.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <asndlib.h>
#include <mp3player.h>

extern void usleep(int s);
static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

const u32 RGB2YCBCR(u8 r1, u8 g1, u8 b1) {
	u8 r2 = r1; u8 g2 = g1; u8 b2 = b1;
	if (r1 < 16) r1 = 16;
	if (g1 < 16) g1 = 16;
	if (b1 < 16) b1 = 16;
	if (r2 < 16) r2 = 16;
	if (g2 < 16) g2 = 16;
	if (b2 < 16) b2 = 16;

	if (r1 > 240) r1 = 240;
	if (g1 > 240) g1 = 240;
	if (b1 > 240) b1 = 240;
	if (r2 > 240) r2 = 240;
	if (g2 > 240) g2 = 240;
	if (b2 > 240) b2 = 240;

	u8 Y1 = ( 77 * r1 + 150 * g1 + 29 * b1) / 256;
	u8 Y2 = ( 77 * r2 + 150 * g2 + 29 * b2) / 256;
	u8 Cb = (112 * (b1 + b2) -  74 * (g1 + g2) - 38 * (r1 + r2)) / 512 + 128;
	u8 Cr = (112 * (r1 + r2) - 94 * (g1 + g2) - 18 * (b1 + b2)) / 512 + 128;

	return Y1 << 24 | Cb << 16 | Y2 << 8 | Cr;
}

void writetoxfb(void* videoBuffer, u32 offset, u32 length, u32 color)
{
	u32 *p = ((u32*)videoBuffer) + offset;
	for(u32 i = 0; i < length; i++) {
		*p++ = color;
	}
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	WPAD_Init();
	ASND_Init();
	MP3Player_Init();

	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	// Initialise the console, required for printf
	console_init(xfb,16,16,rmode->fbWidth-16,rmode->xfbHeight-16,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	//SYS_STDIO_Report(true);

	// Set up the video registers with the chosen mode
	VIDEO_Configure(rmode);

	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb);

	// Clear the framebuffer
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);

	// Make the display visible
	VIDEO_SetBlack(false);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	fatInitDefault();

	
	FILE *af = fopen("sd:/audio.mp3", "rb");
	
	if (!af) {
		printf("Unable to open file\nExiting...");
		exit(0);
	}
	
	fseek(af, 0, SEEK_END);
    long afsize = ftell(af);
	fseek(af, 0, SEEK_SET);
	u8 *audiobuffer = malloc(afsize);
	fread(audiobuffer, 1, afsize, af);
	fclose(af);

	MP3Player_PlayBuffer(audiobuffer, afsize, NULL);
	
	FILE *vf = fopen("sd:/video.yuv", "rb");
	u8 buffer[(320*240) + 2] = {0};

	while (1)
	{
		fread(buffer, 1, 320*240, vf);
		for (size_t y = 0; y < 240; y++)
		{
			for (size_t x = 0; x < 320; x++)
			{
				u8 color = buffer[x + y*320];
				writetoxfb(xfb, x + y*640 , 1, RGB2YCBCR(color, color, color));
				writetoxfb(xfb, x + y*640 + 320 , 1, RGB2YCBCR(color, color, color));
			}	
		}
		fread(buffer, 1, 120*320, vf);
		if (feof(vf)) break;
		VIDEO_WaitVSync();
	}

	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);

	printf("Brought to you by Abdelali221.\n Thanks for watching");

	while(1) {

		// Call WPAD_ScanPads each loop, this reads the latest controller states

		WPAD_ScanPads();

		// WPAD_ButtonsDown tells us which buttons were pressed in this loop
		// this is a "one shot" state which will not fire again until the button has been released
		u32 pressed = WPAD_ButtonsDown(0);

		// We return to the launcher application via exit
		if ( pressed & WPAD_BUTTON_HOME ) {
			exit(0);
		}

		// Wait for the next frame
		VIDEO_WaitVSync();
	}

	return 0;
}