#include "../common/common.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <pthread.h>

#include "audio_util.h"

/*语音识别要求的pcm采样频率*/
#define PCM_SAMPLE_RATE 16000 /* 16KHz */
#define WHITE   FB_COLOR(255,255,255)
#define BLACK   FB_COLOR(0,0,0)

static int touch_fd;

#define SEND_X	(SCREEN_WIDTH-60)
#define SEND_Y	(SCREEN_HEIGHT-60)
#define SEND_W	60
#define SEND_H	60

#define BACKSPACE_X	(SCREEN_WIDTH-90)
#define BACKSPACE_Y	0
#define BACKSPACE_W	90
#define BACKSPACE_H	60

#define RECORD_X	(SCREEN_WIDTH-180)
#define RECORD_Y	0
#define RECORD_W	90
#define RECORD_H	60

#define TIME_X	(SCREEN_WIDTH-100)
#define TIME_Y	60
#define TIME_W	100
#define TIME_H	30



extern volatile sig_atomic_t stop_recording;
static pthread_t record_thread;

static char * send_to_vosk_server(char *file);

static void * record_voice();

static void stop_record_voice();

static int st=0;
static void timer_cb(int period) /*该函数0.5秒执行一次*/
{
	char buf[100];
	sprintf(buf, "%d", st--);
	if(st == -1){
		stop_record_voice();
	}
	else{
		fb_draw_rect(TIME_X, TIME_Y, TIME_W, TIME_H, WHITE);
		fb_draw_border(TIME_X, TIME_Y, TIME_W, TIME_H, BLACK);
		fb_draw_text(TIME_X+2, TIME_Y+20, buf, 24, BLACK);
	}
	fb_update();
	return;
}

static void touch_event_cb(int fd)
{
	int type,x,y,finger;
	type = touch_read(fd, &x,&y,&finger);
	switch(type){
	case TOUCH_PRESS:
		//printf("type=%d,x=%d,y=%d,finger=%d\n",type,x,y,finger);
		if((x>=SEND_X)&&(x<SEND_X+SEND_W)&&(y>=SEND_Y)&&(y<SEND_Y+SEND_H)) {
			printf("Sending!\n");
		}
		if((x>=RECORD_X)&&(x<RECORD_X+RECORD_W)&&(y>=RECORD_Y)&&(y<RECORD_Y+RECORD_H)) {
			if(!stop_recording){
				printf("Recording!\n");
				if (pthread_create(&record_thread, NULL, record_voice, NULL) != 0) {
					perror("pthread_create");
					exit(1);
				}
			}
			else{
				stop_record_voice();
				printf("Voice record stopped!\n");
			}
		}
		if((x>=BACKSPACE_X)&&(x<BACKSPACE_X+BACKSPACE_W)&&(y>=BACKSPACE_Y)&&(y<BACKSPACE_Y+BACKSPACE_H)) {
			printf("BACKING!\n");
		}
		break;
	case TOUCH_ERROR:
		printf("close touch fd\n");
		task_delete_file(fd);
		close(fd);
		break;
	default:
		return;
	}
	fb_update();
	return;
}

int main(int argc, char *argv[])
{
	fb_init("/dev/fb0");
	font_init("./font.ttc");
	audio_record_init("plughw:1,0", PCM_SAMPLE_RATE, 1, 16); //单声道，S16采样   pulse audio服务不知道为什么用不了    NULL

	fb_draw_rect(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,WHITE);
	fb_draw_border(SEND_X, SEND_Y, SEND_W, SEND_H, BLACK);
	fb_draw_text(SEND_X+6, SEND_Y+30, "send", 24, BLACK);
	fb_update();

	fb_draw_border(RECORD_X, RECORD_Y, RECORD_W, RECORD_H, BLACK);
	fb_draw_text(RECORD_X+12, RECORD_Y+30, "record", 24, BLACK);
	fb_update();

	fb_draw_border(BACKSPACE_X, BACKSPACE_Y, BACKSPACE_W, BACKSPACE_H, BLACK);
	fb_draw_text(BACKSPACE_X+20, BACKSPACE_Y+30, "back", 24, BLACK);
	fb_update();

	touch_fd = touch_init("/dev/input/event2");
	task_add_file(touch_fd, touch_event_cb);

	task_loop();
	return 0;
}

/*===============================================================*/	

#define IP "127.0.0.1"
#define PORT 8011

#define print_err(fmt, ...) \
	printf("%d:%s " fmt, __LINE__, strerror(errno), ##__VA_ARGS__);

static char * send_to_vosk_server(char *file)
{
	static char ret_buf[128]; //识别结果

	if((file == NULL)||(file[0] != '/')) {
		print_err("file %s error\n", file);
		return NULL;
	}

	int skfd = -1, ret = -1;
	skfd = socket(AF_INET, SOCK_STREAM, 0);
	if(skfd < 0) {
		print_err("socket failed\n");
		return NULL;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = inet_addr(IP);
	ret = connect(skfd,(struct sockaddr*)&addr, sizeof(addr));
	if(ret < 0) {
		print_err("connect failed\n");
		close(skfd);
		return NULL;
	}

	printf("send wav file name: %s\n", file);
	ret = send(skfd, file, strlen(file)+1, 0);
	if(ret < 0) {
		print_err("send failed\n");
		close(skfd);
		return NULL;
	}

	ret = recv(skfd, ret_buf, sizeof(ret_buf)-1, 0);
	if(ret < 0) {
		print_err("recv failed\n");
		close(skfd);
		return NULL;
	}
	ret_buf[ret] = '\0';
	return ret_buf;
}

static void stop_record_voice(){
	fb_draw_rect(RECORD_X,RECORD_Y,RECORD_W,RECORD_H,WHITE);
	fb_draw_border(RECORD_X, RECORD_Y, RECORD_W, RECORD_H, BLACK);
	fb_draw_text(RECORD_X+12, RECORD_Y+30, "record", 24, BLACK);

	task_delete_timer(1000);
	fb_draw_rect(TIME_X,TIME_Y,TIME_W,TIME_H,WHITE);
	fb_update();
	stop_recording = 0;
}

static void* record_voice(){
	fb_draw_rect(RECORD_X,RECORD_Y,RECORD_W,RECORD_H,WHITE);
	fb_draw_border(RECORD_X, RECORD_Y, RECORD_W, RECORD_H, BLACK);
	fb_draw_text(RECORD_X+22, RECORD_Y+30, "stop", 24, BLACK);
	fb_update();
	stop_recording = 1;

	st=10;
	task_add_timer(1000, timer_cb);

	
	pcm_info_st pcm_info;
	uint8_t *pcm_buf = audio_record(10000, &pcm_info); //录2秒音频

	if(pcm_info.sampleRate != PCM_SAMPLE_RATE) { //实际录音采用率不满足要求时 resample
		uint8_t *pcm_buf2 = pcm_s16_mono_resample(pcm_buf, &pcm_info, PCM_SAMPLE_RATE, &pcm_info);
		pcm_free_buf(pcm_buf);
		pcm_buf = pcm_buf2;
	}

	pcm_write_wav_file(pcm_buf, &pcm_info, "/tmp/test.wav");
	printf("write wav end\n");

	pcm_free_buf(pcm_buf);

	char *rev = send_to_vosk_server("/tmp/test.wav");
	printf("recv from server: %s\n", rev);
	
	return NULL;
}