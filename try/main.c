#include <sys/mman.h>
#include <linux/fb.h>
#include <stdio.h>

#include "../common/common.h"

#define RED		FB_COLOR(255,0,0)
#define ORANGE	FB_COLOR(255,165,0)
#define YELLOW	FB_COLOR(255,255,0)
#define GREEN	FB_COLOR(0,255,0)
#define CYAN	FB_COLOR(0,127,255)
#define BLUE	FB_COLOR(0,0,255)
#define PURPLE	FB_COLOR(139,0,255)
#define WHITE   FB_COLOR(255,255,255)
#define BLACK   FB_COLOR(0,0,0)

int color[9] = {RED,ORANGE,YELLOW,GREEN,CYAN,BLUE,PURPLE,WHITE,BLACK};

int main(int argc, char* argv[])
{
	int row,column,i;
	int32_t start, end;

	fb_init("/dev/fb0");
	fb_draw_rect(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,BLACK);
	fb_update();

	sleep(1);
	fb_draw_line(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, RED);
	fb_draw_line(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH - 1, 0, RED);
	fb_update();
	return 0;
}