#include "comshead.h"
#include "base/pool.h"
#include "srvwork/workinter.h"
#include "sysdata.h"
#include "host/voices/callvoices.h"
#include "host/voices/wm8960i2s.h"
#include "net/network.h"
#include "host/ap_sta.h"
#include "host/StreamPlay/StreamFile.h"
#include "host/voices/gpio_7620.h"
#include "host/sdcard/musicList.h"
#include "config.h"
#include "host/studyvoices/prompt_tone.h"
#include "uart/uart.h"
#include "host/studyvoices/std_worklist.h"
#ifdef WORK_INTER
#include "srvwork/workinter.h"
#endif
#include "log.h"
static WorkQueue *DownEvent=NULL;
static unsigned char mainQueLock=0;

/*
@添加播放/下载 歌曲时间
@data :添加消息数据  msgSize:消息的大小
@ 返回:0添加成功 -1 添加失败
*/
int AddDownEvent(const char *data,int msgSize){
	return putMsgQueue(DownEvent,data, msgSize);
}
//获取播放歌曲事件队列数
int getplayEventNum(void){
	return getWorkMsgNum(DownEvent);
}

//设置主线程队列资源锁
void SetMainQueueLock(unsigned char lock){
	mainQueLock=lock;
}
//退出清除资源
void CleanSystemResources(void){
	CleanMtkPlatfrom76xx();
	clean_videoServer();
	pool_destroy();
	AddDownEvent((const char *)"baibai",QUIT_MAIN);
	printf("clean resources finished \n");
}

/*
* 加载传进来的参数,提取qtts 系统语音路径等
*/
static void loadLocalServer(int argc,char *argv[]){
	int i;
	int sleeptime=3;
	char *token=NULL;
	char *user_id  = NULL;
	
	if(argc<4){
		printf("localserver -qttspath /home/ -t 3 -userId xxxxxx -token xxxxxx -v xxx\n");
		exit(1);
	}	
	memset(&sysMes,0,sizeof(SysMessage));
	for(i=0; i<argc; i++){
		int lastarg = i==argc-1;
		if(!strcmp(argv[i],"-qttspath") && !lastarg){
			char *qttspath = argv[i+1];
			memcpy(sysMes.localVoicesPath,qttspath,strlen(qttspath));
		}else if(!strcmp(argv[i],"-t") && !lastarg){
      	  	printf("i :%d sleeptime: %s\n",argc,argv[i+1]);
			sleeptime = atoi(argv[i+1]);
		}else if(!strcmp(argv[i],"-userId") && !lastarg){
			user_id = argv[i+1];
		}else if(!strcmp(argv[i],"-token") && !lastarg){
			token = argv[i+1];
		}else if(!strcmp(argv[i],"-v") && !lastarg){
			WriteLocalserver_Version((const char *)argv[i+1]);
		}
	}
	
	time_t t;
	sysMes.localplayname=0;			//本地播放目录
	sysMes.netstate=NETWORK_UNKOWN;	//开机不属于未知网络状态
	sysMes.Playlocaltime=time(&t);
	set_pthread_sigblock();
	pool_init(4);	
	InitTuling((const char *) user_id,(const char *) token);	//userId需要保存到路由表当中 ，token 也需要保存
	InitMtkPlatfrom76xx();
	sleep(sleeptime);	//增加一定睡眠时间，防止加载sdcard和sock冲突，导致udp sock不能通信
	DownEvent = initQueue();
	
#ifdef WORK_INTER
	init_interface(pasreInputCmd);
#endif	//end WORK_INTER
	init_videoServer();
	init_Uart(UartEventcallFuntion,ack_batteryCtr);	//初始化串口
	
#ifdef	LED_LR
	led_lr_oc(closeled);
#endif
	srand((unsigned)time(NULL));	//取随机数基数
	mkdir(CACHE_WAV_PATH,777);
}
#ifdef LOCAL_MP3
//自动播放下一首歌曲 musicType 播放歌曲类型(用来区分目录标识)
static void autoPlayNextMusic(unsigned char musicType){
	setAutoPlayMusicTime();
	switch(musicType){
		case mp3:
			Create_playMusicEvent((const void *)"mp3", PLAY_NEXT);
			break;
		case story:
			Create_playMusicEvent((const void *)"story", PLAY_NEXT);
			break;
		case english:
			Create_playMusicEvent((const void *)"english", PLAY_NEXT);
			break;
		case guoxue:
			Create_playMusicEvent((const void *)"guoxue", PLAY_NEXT);
			break;
		case xiai:
			Create_playMusicEvent((const void *)XIAI_DIR, PLAY_NEXT);
			break;
#if defined(HUASHANG_JIAOYU)			
		case huashang:
			Create_playMusicEvent((const void *)HUASHANG_GUOXUE_DIR, PLAY_NEXT);
			break;
#endif			
		default:
			sysMes.localplayname=0;
		break;
	}	
}
#endif
//主线程添加网络歌曲到队列当中播放
static void Main_Thread_AddplayUrlMusic(const char *msg){
#ifdef PALY_URL_SD
		PlayUrl((const void *)msg);
#else
		NetStreamDownFilePlay((const void *)msg);
#endif
#if defined(HUASHANG_JIAOYU)
	if(GetStreamPlayState()==MUSIC_SINGLE_LIST){	//单曲循环
		CreatePlayListMuisc((const char *)msg,PLAY_MUSIC_NETWORK);
	}else{
		free((void *)msg);
	}
#elif defined(QITUTU_SHI)
		free((void *)msg);
#endif
}
//主线程添加本地到队列当中播放
static void Main_Thread_AddPlayLocalSdcard_Music(const char *msg){
	playLocalMp3(msg);
#if defined(HUASHANG_JIAOYU)	
	if(GetStreamPlayState()==MUSIC_SINGLE_LIST){	//单曲循环
		CreatePlayListMuisc((const char *)msg,PLAY_MUSIC_SDCARD);
	}else{
		if(getEventNum()==0&&getWorkMsgNum(DownEvent)==0){
			autoPlayNextMusic(sysMes.localplayname);
		}	
	}
#elif defined(QITUTU_SHI)
	if(getEventNum()==0&&getWorkMsgNum(DownEvent)==0){
		autoPlayNextMusic(sysMes.localplayname);
	}	
#endif
	free((void *)msg);
	usleep(1000);
}
static void Main_Thread_playTuLingMusic(HandlerText_t *hand){
	if(hand->EventNums!=GetCurrentEventNums()){
		goto exit0;
	}
	start_event_play_url(); 	
#ifdef PALY_URL_SD
	PlayUrl((const void *)hand->data);
#else
	NetStreamDownFilePlay((const void *)hand->data);
#endif
exit0:
	free((void *)hand->data);
	free((void *)hand);
}
//检查文件锁，防止配网、启动联网脚本导致多次启动进程
static void checkFileLock(void){
	if (access(LOCAL_SERVER_FILE_LOCK, 0) < 0){
		FILE *fp =fopen(LOCAL_SERVER_FILE_LOCK, "w+");
		if(fp){	
			fclose(fp);
		}
	}
	else{
		printf("please delete %s \n",LOCAL_SERVER_FILE_LOCK);
		exit(1);
	}
}

int main(int argc, char **argv){   
	checkFileLock();
	loadLocalServer(argc,argv);
	char *msg=NULL;
	int event=0;
	showFacePicture(CONNECT_WIFI_ING_PICTURE);
	while(1){
		getMsgQueue(DownEvent,&msg,&event);
		if (mainQueLock == MAIN_QUEUE_LOCK){
			free((void *)msg);
			usleep(100);
			continue;
		}
		switch(event){
			case URL_VOICES_EVENT:	//url播放
				Main_Thread_AddplayUrlMusic((const char *)msg);
				break;
			case TULING_URL_VOICES:	//播放图灵 语音点歌、故事 url文件
				Main_Thread_playTuLingMusic((HandlerText_t *)msg);
				break;
			case TULING_URL_MAIN:	//播放图灵 tts文件
				if(PlayTulingText((HandlerText_t *)msg)){	//异常退出，需要清理后面的url播放事件
					SetMainQueueLock(MAIN_QUEUE_LOCK);		//清理后面mp3播放
				}
				break;
			case LOCAL_MP3_EVENT:	//本地播放
				Main_Thread_AddPlayLocalSdcard_Music((const char *)msg);
				break;		
#ifdef PALY_URL_SD
			case WEIXIN_DOWN_MP3_EVENT:	//微信端下载歌曲事件	
				HandlerWeixinDownMp3((const void *)msg);
				free((void *)msg);
				break;
#endif				
			case UPLOAD_TULING_EVENT:	//将耗时操作的上传接口放到队列当中
				break;
			case QUIT_MAIN:
				printf("end main !!!\n");
				goto exit0;
		}
	}
exit0:
	return 0;
}
