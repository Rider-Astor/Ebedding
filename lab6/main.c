#include "../common/common.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <pthread.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#include "audio_util.h"
#include "main.h"

CURL *curl;
CURLcode curlres;
const char *pythonurl = "http://localhost:5001/api/python-service";
char pythondata[600];
char response[600];
void * pthreadres;

static int touch_fd;

static char * chatlog[100];
static int role[100];
static int logpointer = 0;
static int logcnt = 0;

static char text[TEXTSIZE];
static int textpointer = 0;
static int textlen = 0;

extern volatile sig_atomic_t stop_recording;
static pthread_t record_thread;

static char * send_to_vosk_server(char *file);

static void * record_voice();

static void stop_record_voice();

static void move_text(char * src, char * dst, int len);

size_t write_callback(void *ptr, size_t size, size_t nmemb, char *data);

void request_init();

void parse_json_response(char *json_str);

char printbuf[TEXTLINESIZE * 3 + 1];
static void loadlog(){
	fb_draw_rect(0, 0, SCREEN_WIDTH - 180, SCREEN_HEIGHT - 120, WHITE);
	for(int i = 0;((i + logpointer) < logcnt) && (i < 5);i ++){
		if(!role[i + logpointer]) {
			fb_draw_text(5, i * LOG_H + 30, "U:", 24, BLACK);
		}
		else{
			fb_draw_text(5, i * LOG_H + 30, "A:", 24, BLACK);
		}
		int printpointer = 0, printonce = 0, line = 0, logcontlen = strlen(chatlog[i + logpointer]);
		while(printpointer < logcontlen){
			printonce = TEXTLINESIZE * 3 + printpointer >= logcontlen ? logcontlen - printpointer : TEXTLINESIZE * 3;
			memset(printbuf, 0, sizeof(printbuf));
			strncpy(printbuf, chatlog[i + logpointer] + printpointer, printonce);
			fb_draw_text(65, i * LOG_H + 30 + LINE_H * line, printbuf, 24, BLACK);
			line++;
			printpointer += printonce;
		}
	}
}

static void loadpointer(){
	int line = textpointer / (TEXTLINESIZE * 3), locate = (textpointer % (TEXTLINESIZE * 3)) / 3;
	fb_draw_text(TEXT_X + 2 + locate * 24, TEXT_Y + 30 + LINE_H * line, "|", 24, BLACK);
	fb_update();
}

static void loadtext(){
	text[textlen] = '\0';
	fb_draw_rect(TEXT_X, TEXT_Y, TEXT_W, TEXT_H, WHITE);
	fb_draw_border(TEXT_X, TEXT_Y, TEXT_W, TEXT_H, BLACK);
	int printpointer = 0, printonce = 0, line = 0;
	while(printpointer < textlen){
		printonce = TEXTLINESIZE * 3 + printpointer >= textlen ? textlen - printpointer : TEXTLINESIZE * 3;
		memset(printbuf, 0, sizeof(printbuf));
		strncpy(printbuf, text + printpointer, printonce);
		fb_draw_text(TEXT_X + 5, TEXT_Y + 30 + LINE_H * line, printbuf, 24, BLACK);
		line++;
		printpointer += printonce;
	}
	fb_update();
}

static void backspace(){
	if(textpointer >= 3){//UTF-8 一个汉字占3字节
		strncpy(text + textpointer - 3, text + textpointer, textlen - textpointer);
		textpointer -= 3;
		textlen -= 3;
		text[textlen] = text[textlen + 1] = text[textlen + 2] = '\0';
	}
	printf("%s\n", text);
	loadtext();
	loadpointer();
	fb_update();
}

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
		if((x>=SEND_X)&&(x<SEND_X+SEND_W)&&(y>=SEND_Y)&&(y<SEND_Y+SEND_H)) {
			if (pthread_join(record_thread, &pthreadres) != 0) {
				perror("pthread_join失败");
			}
			if(textlen == 0) break;
			chatlog[logcnt] = (char*) malloc((textlen + 1) * sizeof(char));
			strcpy(chatlog[logcnt], text);
			role[logcnt] = 0;  //USER
			logcnt ++;
			loadlog();
			sprintf(pythondata, "{\"question\":\"%s\"}", text);
			memset(text, 0, sizeof(text));
			textpointer = textlen = 0;
			loadtext();
			loadpointer();

			memset(response, 0, sizeof(response));
			curlres = curl_easy_perform(curl);

			// 检查请求是否成功
			if(curlres != CURLE_OK) {
				fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(curlres));
			} else {
				// 打印服务器返回的数据
				parse_json_response(response);
				chatlog[logcnt] = (char*) malloc((strlen(response) + 1) * sizeof(char));
				strcpy(chatlog[logcnt], response);
				role[logcnt] = 1;
				logcnt ++;
				loadlog();
			}
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
			backspace();
		}
		if((x>=MOVELF_X)&&(x<MOVELF_X+MOVELF_W)&&(y>=MOVELF_Y)&&(y<MOVELF_Y+MOVELF_H)) {
			if(textpointer >= 3) textpointer -= 3;
			loadtext();
			loadpointer();
		}
		if((x>=MOVERT_X)&&(x<MOVERT_X+MOVERT_W)&&(y>=MOVERT_Y)&&(y<MOVERT_Y+MOVERT_H)) {
			if(textpointer + 3 <= textlen) textpointer += 3;
			loadtext();
			loadpointer();
		}
		if((x>=UP_X)&&(x<UP_X+UP_W)&&(y>=UP_Y)&&(y<UP_Y+UP_H)) {
			if(logpointer >= 1) logpointer --;
			loadlog();  // TODO
		}
		if((x>=DOWN_X)&&(x<DOWN_X+DOWN_W)&&(y>=DOWN_Y)&&(y<DOWN_Y+DOWN_H)) {
			if(logpointer + 5 < logcnt) logpointer ++;
			loadlog();  // TODO
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
	request_init();

	fb_draw_rect(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,WHITE);
	fb_draw_border(SEND_X, SEND_Y, SEND_W, SEND_H, BLACK);
	fb_draw_text(SEND_X+10, SEND_Y+80, "发送", 48, BLACK);

	fb_draw_border(RECORD_X, RECORD_Y, RECORD_W, RECORD_H, BLACK);
	fb_draw_text(RECORD_X+12, RECORD_Y+40, "record", 24, BLACK);

	fb_draw_border(BACKSPACE_X, BACKSPACE_Y, BACKSPACE_W, BACKSPACE_H, BLACK);
	fb_draw_text(BACKSPACE_X+20, BACKSPACE_Y+40, "back", 24, BLACK);

	fb_draw_border(MOVELF_X, MOVELF_Y, MOVELF_W, MOVELF_H, BLACK);
	fb_draw_text(MOVELF_X+22, MOVELF_Y+40, "左移", 24, BLACK);

	fb_draw_border(MOVERT_X, MOVERT_Y, MOVERT_W, MOVERT_H, BLACK);
	fb_draw_text(MOVERT_X+22, MOVERT_Y+40, "右移", 24, BLACK);

	fb_draw_border(UP_X, UP_Y, UP_W, UP_H, BLACK);
	fb_draw_text(UP_X+22, UP_Y+40, "向上", 24, BLACK);

	fb_draw_border(DOWN_X, DOWN_Y, DOWN_W, DOWN_H, BLACK);
	fb_draw_text(DOWN_X+22, DOWN_Y+40, "向下", 24, BLACK);

	fb_draw_line(0, SEND_Y, SEND_X, SEND_Y, BLACK);
	fb_draw_line(RECORD_X, MOVELF_Y + 80, RECORD_X, SEND_Y, BLACK);

	loadpointer();
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
	fb_draw_text(RECORD_X+12, RECORD_Y+40, "record", 24, BLACK);

	task_delete_timer(1000);
	fb_draw_rect(TIME_X,TIME_Y,TIME_W,TIME_H,WHITE);
	fb_update();
	stop_recording = 0;
}

static void* record_voice(){
	fb_draw_rect(RECORD_X,RECORD_Y,RECORD_W,RECORD_H,WHITE);
	fb_draw_border(RECORD_X, RECORD_Y, RECORD_W, RECORD_H, BLACK);
	fb_draw_text(RECORD_X+22, RECORD_Y+40, "stop", 24, BLACK);
	fb_update();
	stop_recording = 1;

	st=9;  //这个timer设计0s时不会调用，但是不重要，只是起提示作用，不作精细化处理
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
	int i = 0, revlen = 0;
	while(rev[i] != '\0'){
		if (rev[i] != ' ') rev[revlen++] = rev[i];  // 如果不是空格，将字符复制到新位置
		i++;
	}
	rev[revlen] = '\0';
	printf("recv from server: %s\n", rev);
	//处理至输入框
	move_text(text + textpointer, text + textpointer + revlen, textlen - textpointer);
	move_text(rev, text + textpointer, revlen);
	textlen += revlen;
	textpointer += revlen;
	text[textlen] = '\0';
	loadtext();
	loadpointer();
	return NULL;
}

static void move_text(char * src, char * dst, int len){
	for(int i = len - 1;i >= 0; i--){
		dst[i] = src[i];
	}
}


size_t write_callback(void *ptr, size_t size, size_t nmemb, char *data) {
    strcat(data, ptr);  // 将返回的内容拼接到 data 中
    return size * nmemb;
}

void request_init(){
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, pythonurl);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, pythondata);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
        printf("CURL INIT OK\n");
    }
    else{
        printf("CURL INIT ERROR\n");
    }
}

void parse_json_response(char *json_str) {
    // 解析 JSON 字符串
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL) {
        printf("Error parsing JSON\n");
        return;
    }

    // 提取 "response" 字段
    cJSON *json_res = cJSON_GetObjectItem(json, "response");
    if (json_res != NULL && cJSON_IsString(json_res)) {
       	strcpy(json_str, json_res->valuestring);
		printf("%s\n", json_str);
    } else {
        printf("Response field not found or not a string\n");
    }

    // 释放 JSON 对象
    cJSON_Delete(json);
}