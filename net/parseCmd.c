#include "comshead.h"
#include "base/pool.h"
#include "base/cJSON.h"
#include "network.h"
#include "mplay.h"
#include "StreamFile.h"
#include "host/voices/wm8960i2s.h"
#include "../host/studyvoices/qtts_qisc.h"
#include "../host/voices/gpio_7620.h"
#include "host/ap_sta.h"
#include "host/voices/callvoices.h"
#include "sysdata.h"
#include "uart/uart.h"
#include "config.h"

#define STREAM_EXIT				MAD_EXIT	//停止	
#define STREAM_PLAY 			MAD_PLAY	//播放
#define STREAM_PAUSE			MAD_PAUSE	//暂停
#define STREAM_NEXT				MAD_NEXT

#define 	GET_NET_STATE	29	//获取网络状态

//#define DBG_TCP
#ifdef DBG_TCP
#define DEBUG_TCP(fmt, args...) printf("network: " fmt, ## args)
#else   
#define DEBUG_TCP(fmt, args...) { }
#endif	//end DBG_AP_STA

#define VOL_DATA(x) (x-90)*4

unsigned char gpio_look=0;

static int SendTo(int sockfd,char *data,int size,struct sockaddr_in *peer){
	char *cachedata = (char *)calloc(1,size+16);
	if(cachedata==NULL){
		perror("calloc error !!!");
		return -1;
	}
	snprintf(cachedata,16,"%s%d%s","head:",size,":");
	memcpy(cachedata+16,data,size);
	sendto(sockfd, (char *)cachedata, size+16, 0,(struct sockaddr*)peer, sizeof(struct sockaddr));
	free(cachedata);
	return 0;
}
static void recv_brocastCtr(int sockfd,struct sockaddr_in *peer,char *recvdata){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	char IP[20]={0};
	GetNetworkcardIp("apcli0",IP);
	//GetNetworkcardIp("eth2.2",IP);
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "brocast");
	cJSON_AddStringToObject(pItem, "ip",IP); 
	cJSON_AddNumberToObject(pItem, "port", 20000);
	cJSON_AddStringToObject(pItem, "status","ok");
	szJSON = cJSON_Print(pItem);
	printf("recv_brocastCtr :\n%s(%d)\n",szJSON,strlen(szJSON));
	int i;
	for(i=0;i<4;i++){
		SendTo(sockfd, (char *)szJSON, strlen(szJSON),peer);
		usleep(10*1000);
	}
	cJSON_Delete(pItem);
	free(szJSON);
}

static void test_brocastCtr(int sockfd,struct sockaddr_in *peer,char *recvdata){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	char IP[20]={0};
	GetNetworkcardIp("apcli0",IP);
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "TestNet");
	cJSON_AddStringToObject(pItem, "ip",IP); 
	cJSON_AddNumberToObject(pItem, "port", 20000);
	if(!strcmp(IP,""))
	{
		cJSON_AddStringToObject(pItem, "status","failed");
	}else{
		cJSON_AddStringToObject(pItem, "status","ok");
	}
	szJSON = cJSON_Print(pItem);
	printf("test_brocastCtr :\n%s(%d)\n",szJSON,strlen(szJSON));
	int i;
	for(i=0;i<2;i++){
		SendTo(sockfd, (char *)szJSON, strlen(szJSON),peer);
		usleep(10*1000);
		}
	cJSON_Delete(pItem);
	free(szJSON);
}

/*
@函数功能:	音量加减处理以及返回函数
@参数:	size 音量加减标志位
@		0	音量减
@		1	音量加
*/
void ack_VolCtr(char *dir,int data){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "vol");
	cJSON_AddStringToObject(pItem, "dir", dir);
	cJSON_AddNumberToObject(pItem, "data", VOL_DATA(data));
	cJSON_AddStringToObject(pItem, "status","ok");
	szJSON = cJSON_Print(pItem);		
	DEBUG_TCP("ack_VolCtr: %s\n",szJSON);
	sendAll_Ack(szJSON,strlen(szJSON));
	cJSON_Delete(pItem);
	free(szJSON);
}
/*
@函数功能:	开关机返回函数
@参数:	size 男女音标志位
@		0	开
@		1	关
@		recvdata 开关机时间
*/
void ack_hostCtr(int sockfd,char *recvdata,char *type){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	char buffer[32];
	memset(buffer,0,32);
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "host");
	cJSON_AddStringToObject(pItem, "type", type);
	cJSON_AddStringToObject(pItem, "time",recvdata);
	cJSON_AddStringToObject(pItem, "status","ok");
	szJSON = cJSON_Print(pItem);
	DEBUG_TCP("ack_hostCtr: %s\n",szJSON);
	sendAll_Ack(szJSON,strlen(szJSON));
	cJSON_Delete(pItem);
	free(szJSON);
}
/*
@函数功能:	电池电量返回函数
@参数:	recvdata 电池电量
*/
void ack_batteryCtr(int recvdata,int power){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "battery");
	cJSON_AddNumberToObject(pItem, "state",recvdata);
	cJSON_AddNumberToObject(pItem, "power",power);
	cJSON_AddStringToObject(pItem, "status","ok");
	szJSON = cJSON_Print(pItem);
	DEBUG_TCP("ack_batteryCtr: %s\n",szJSON);
	sendAll_Ack(szJSON,strlen(szJSON));
	cJSON_Delete(pItem);
	free(szJSON);
}

/*
@函数功能:	所有信息返回函数
@参数:	recvdata 电池电量
*/
void ack_alluserCtr(const int sockfd,int state,int power){
	char stropenTime[10]={0};
	char strcloseTime[10]={0};
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "sys");
	cJSON_AddNumberToObject(pItem, "state",state);
	cJSON_AddNumberToObject(pItem, "power",power);
	Get_OpenCloseTime_formRouteTable(OPEN_TIME,stropenTime);
	cJSON_AddStringToObject(pItem, "openTime",stropenTime);
	Get_OpenCloseTime_formRouteTable(CLOSE_TIME,strcloseTime);
	cJSON_AddStringToObject(pItem, "closeTime",strcloseTime);
	cJSON_AddStringToObject(pItem, "status","ok");
	szJSON = cJSON_Print(pItem);
	DEBUG_TCP("ack_alluserCtr: %s\n",szJSON);
	send_ctrl_ack(sockfd,szJSON,strlen(szJSON));
	SendtoaliyunServices(szJSON,strlen(szJSON));	//发送给微信
	cJSON_Delete(pItem);
	free(szJSON);
}
static void CreateState(cJSON* pItem,unsigned char playState){
	if(playState==STREAM_EXIT){
		cJSON_AddStringToObject(pItem, "state","stop");
	}else if(playState==STREAM_PLAY){
		cJSON_AddStringToObject(pItem, "state","play");
	}else if(playState==STREAM_PAUSE){
		cJSON_AddStringToObject(pItem, "state","pause");
	}else if(playState==STREAM_NEXT){
		cJSON_AddStringToObject(pItem, "state","switch");
	}
}
void ack_allplayerCtr(void *data,Player_t *player){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	int sockfd = *(int *)data;
	
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "mplayer");
	CreateState(pItem,player->playState);

	cJSON_AddStringToObject(pItem, "url",player->playfilename);
	cJSON_AddNumberToObject(pItem, "lock",gpio_look);
	cJSON_AddNumberToObject(pItem, "vol",VOL_DATA(player->vol));
	cJSON_AddStringToObject(pItem, "name",player->musicname);
	cJSON_AddNumberToObject(pItem, "time",(int)player->musicTime);
	cJSON_AddNumberToObject(pItem, "progress",player->progress);
	cJSON_AddStringToObject(pItem, "status","ok");
	szJSON = cJSON_Print(pItem);
	DEBUG_TCP("ack_allplayerCtr: %s\n",szJSON);
	send_ctrl_ack(sockfd,szJSON,strlen(szJSON));
	cJSON_Delete(pItem);
	free(szJSON);
}

void ack_playCtr(int nettype,Player_t *play,unsigned char playState){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "mplayer");
	CreateState(pItem,playState);
	if(playState==STREAM_EXIT){
#ifndef CLOSE_VOICE 
		Mute_voices(MUTE);
#endif
		CleanI2S_PlayCachedata();
	}
	cJSON_AddStringToObject(pItem, "url",play->playfilename);
	cJSON_AddStringToObject(pItem, "name",play->musicname);
	cJSON_AddNumberToObject(pItem, "time",(int)play->musicTime);
	cJSON_AddNumberToObject(pItem, "progress",(int)play->progress);
	cJSON_AddStringToObject(pItem, "status","ok");
	szJSON = cJSON_Print(pItem);
	//DEBUG_TCP("ack_playCtr: %s\n",szJSON);
	if(nettype)
		UdpAll_Ack(szJSON,strlen(szJSON));
	else
		sendAll_Ack(szJSON,strlen(szJSON));
	cJSON_Delete(pItem);
	free(szJSON);
}	

void ack_gpioCtr(int recvdata){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "lock");
	cJSON_AddNumberToObject(pItem, "state",recvdata);
	cJSON_AddStringToObject(pItem, "status","ok");
	szJSON = cJSON_Print(pItem);
	DEBUG_TCP("ack_gpioCtr: %s\n",szJSON);
	sendAll_Ack(szJSON,strlen(szJSON));
	cJSON_Delete(pItem);
	free(szJSON);
}

void GetNetState(void){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "ServerWifi");
	cJSON_AddNumberToObject(pItem, "event",GET_NET_STATE);
	szJSON = cJSON_Print(pItem);
	SendtoServicesWifi(szJSON,strlen(szJSON));
	cJSON_Delete(pItem);
	free(szJSON);
}
void uploadVoicesToaliyun(const char *filename,int fileSize){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "speek");
	cJSON_AddStringToObject(pItem, "filename",filename);
	cJSON_AddNumberToObject(pItem, "fileSize",fileSize);
	szJSON = cJSON_Print(pItem);
	SendtoaliyunServices(szJSON,strlen(szJSON));
	cJSON_Delete(pItem);
	free(szJSON);
}
void BindDevToaliyun(void){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "binddev");
	cJSON_AddStringToObject(pItem, "list","abc");
	cJSON_AddStringToObject(pItem, "status","ok");
	szJSON = cJSON_Print(pItem);
	SendtoaliyunServices(szJSON,strlen(szJSON));
	cJSON_Delete(pItem);
	free(szJSON);
}
void Ack_CallDev(int recvdata){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "call");
	cJSON_AddStringToObject(pItem, "status","cancel");
	szJSON = cJSON_Print(pItem);
	DEBUG_TCP("ack_gpioCtr: %s\n",szJSON);
	SendtoaliyunServices(szJSON,strlen(szJSON));	//发送给微信
	cJSON_Delete(pItem);
	free(szJSON);
}
#ifdef CLOCKTOALIYUN
void CloseSystemSignToaliyun(void){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "clock");
	cJSON_AddStringToObject(pItem, "status","close");
	szJSON = cJSON_Print(pItem);
	SendtoaliyunServices(szJSON,strlen(szJSON));
	test_clock_Interfaces((const char * )szJSON);
	cJSON_Delete(pItem);
	free(szJSON);
}
void SetClockToaliyun(unsigned char clocknum,unsigned char state,const char *time,const char *ringPath){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "clock");
	cJSON_AddStringToObject(pItem, "status","set");
	cJSON_AddNumberToObject(pItem, "clocknum",clocknum);
	cJSON_AddNumberToObject(pItem, "state",state);
	cJSON_AddStringToObject(pItem, "time",time);
	cJSON_AddStringToObject(pItem, "ringPath",ringPath);
	szJSON = cJSON_Print(pItem);
	SendtoaliyunServices(szJSON,strlen(szJSON));
	cJSON_Delete(pItem);
	free(szJSON);
}
#endif
//清除微信绑定的用户
void ResetWeixinBindUserMessage(void){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "cleanbind");
	szJSON = cJSON_Print(pItem);
	SendtoaliyunServices(szJSON,strlen(szJSON));
	cJSON_Delete(pItem);
	free(szJSON);
}
void handler_CtrlMsg(int sockfd,char *recvdata,int size,struct sockaddr_in *peer){
	cJSON * pJson = cJSON_Parse(recvdata);
	if(NULL == pJson){
		return ;
	}
	//DEBUG_TCP("handler_CtrlMsg = %s\n",recvdata);
	cJSON * pSub = cJSON_GetObjectItem(pJson, "handler");
	if(NULL == pSub){
		DEBUG_TCP("get json data  failed\n");
		goto exit;
	}
	if(!strcmp(pSub->valuestring,"vol")){
		// get number from json
		DEBUG_TCP("handler_CtrlMsg = %s\n",recvdata);
		pSub = cJSON_GetObjectItem(pJson, "dir");
		if(NULL == pSub){
			DEBUG_TCP("get vol failed\n");
			goto exit;
		}
		DEBUG_TCP("dir = %s\n", pSub->valuestring);
		if(!strcmp(pSub->valuestring,"add")){
			Setwm8960Vol(VOL_ADD,0);
			ack_VolCtr("add",GetVol());//----------->音量加
		}else if (!strcmp(pSub->valuestring,"sub")){
			Setwm8960Vol(VOL_SUB,0);
			ack_VolCtr("sub",GetVol());//----------->音量减
		}else if (!strcmp(pSub->valuestring,"no")){
			Setwm8960Vol(VOL_APP_SET,cJSON_GetObjectItem(pJson, "data")->valueint);
			ack_VolCtr("no",GetVol());//----------->设置固定音量
		}
	}//end vol 音量大小
	else if(!strcmp(pSub->valuestring,"lock")){
		pSub = cJSON_GetObjectItem(pJson, "state");
		if(NULL == pSub){
			DEBUG_TCP("get vol failed\n");
			goto exit;
		}
		if(pSub->valueint==0){
			gpio_look=0;
			enable_gpio();//----------->解锁
			ack_gpioCtr(0);
		}else if (pSub->valueint==1){
			disable_gpio();//----------->上锁
			gpio_look=1;
			ack_gpioCtr(1);
		}
	}//end lock 按键锁
	else if(!strcmp(pSub->valuestring,"mplayer")){
		pSub = cJSON_GetObjectItem(pJson, "state");
		if(NULL == pSub){
			DEBUG_TCP("get vol failed\n");
			goto exit;
		}
		if(!strcmp(pSub->valuestring,"get")){
			getStreamState(&sockfd,ack_allplayerCtr);//----------->app登陆获取播放器信息
		}else if(!strcmp(pSub->valuestring,"switch")){
			//mute_recorde_vol(UNMUTE);
			Player_t *player = (Player_t *)calloc(1,sizeof(Player_t));
			if(player==NULL){
				perror("calloc error !!!");
				return;
			}
			char *musicname=NULL;
			if(cJSON_GetObjectItem(pJson, "name")!=NULL){
				musicname=cJSON_GetObjectItem(pJson, "name")->valuestring;
			}
			if(player){					//新增加json协议，用于同步界面 2016-10-11 14:00
				snprintf(player->playfilename,128,"%s",cJSON_GetObjectItem(pJson, "url")->valuestring);
				if(musicname!=NULL){
					snprintf(player->musicname,48,"%s",musicname);
					player->musicTime = cJSON_GetObjectItem(pJson, "time")->valueint;
				}

				__AddNetWork_UrlForPaly(player);
			}
		}else if (!strcmp(pSub->valuestring,"pause")){
			//mute_recorde_vol(MUTE);
			StreamPause();
		}else if (!strcmp(pSub->valuestring,"stop")){
			//mute_recorde_vol(MUTE);
			Create_CleanUrlEvent();
		}else if (!strcmp(pSub->valuestring,"play")){
			//mute_recorde_vol(UNMUTE);
			StreamPlay();
		}else if (!strcmp(pSub->valuestring,"seekto")){
			seekToStream(cJSON_GetObjectItem(pJson, "progress")->valueint);
		}
	}//end mplayer 播放器信息
	else if(!strcmp(pSub->valuestring,"host")){
		pSub = cJSON_GetObjectItem(pJson, "type");
		if(NULL == pSub){
			DEBUG_TCP("get vol failed\n");
			goto exit;
		}
		if(!strcmp(pSub->valuestring,"open")){
			//定时开机
			char *stropenTime = cJSON_GetObjectItem(pJson, "time")->valuestring;
			DEBUG_TCP("stropenTime = %s\n",stropenTime);
			Save_OpenCloseTime_toRouteTable(OPEN_TIME,stropenTime);
			ack_hostCtr(sockfd,stropenTime,"open");
			
			SocSendMenu(3,0);
			usleep(100*1000);
			SocSendMenu(1,stropenTime);//----------->定时开机
			usleep(100*1000);
		}else if (!strcmp(pSub->valuestring,"close")){
			//定时关机
			char *strcloseTime = cJSON_GetObjectItem(pJson, "time")->valuestring;
			DEBUG_TCP("strcloseTime = %s\n",strcloseTime);
			Save_OpenCloseTime_toRouteTable(CLOSE_TIME,strcloseTime);
			ack_hostCtr(sockfd,strcloseTime,"close");
			
			SocSendMenu(3,0);
			usleep(100*1000);
			SocSendMenu(MUC_CLOSE_SYSTEM,strcloseTime);//----------->定时关机
			usleep(100*1000);
		}
	}//end host 定时开关机
	
	//----------------------------------> udp msg	
	else if(!strcmp(pSub->valuestring,"brocast")){
		// get number from json
		pSub = cJSON_GetObjectItem(pJson, "ip");
		if(NULL == pSub)
		{
			DEBUG_TCP("get vol failed\n");
			goto exit;
		}
		if(!strcmp(pSub->valuestring,"null"))
		{
			recv_brocastCtr(sockfd,peer,recvdata);
		}
	}//  end brocast	
#ifdef DOWN_IMAGE
	else if(!strcmp(pSub->valuestring,"updateHost")){	//----------->由版本监测进程发送过来	
		pSub = cJSON_GetObjectItem(pJson, "status");
		if(!strcmp(pSub->valuestring,"newversion")){	//有新版本，需要更新
			Create_PlaySystemEventVoices(UPDATA_NEW_PLAY);
		}else if(!strcmp(pSub->valuestring,"start")){	//正在下载固件
			Create_PlaySystemEventVoices(DOWNLOAD_ING_PLAY);
		}else if(!strcmp(pSub->valuestring,"error")){	//下载固件错误
			Create_PlaySystemEventVoices(DOWNLOAD_ERROE_PLAY);
		}else if(!strcmp(pSub->valuestring,"end")){ 	//下载固件结束
			Create_PlaySystemEventVoices(DOWNLOAD_END_PLAY);
		}else if(!strcmp(pSub->valuestring,"progress")){		//下载进度
			pSub = cJSON_GetObjectItem(pJson, "value");
			if(pSub->valueint==25){
				Create_PlaySystemEventVoices(DOWNLOAD_25_PLAY);			
			}else if(pSub->valueint==50){
				Create_PlaySystemEventVoices(DOWNLOAD_50_PLAY);	
			}else if(pSub->valueint==75){
				Create_PlaySystemEventVoices(DOWNLOAD_75_PLAY);
			}
		}
	}
#endif	
	//  end updateHost
	else if(!strcmp(pSub->valuestring,"updateImage")){		
		pSub = cJSON_GetObjectItem(pJson, "status");
		if(!strcmp(pSub->valuestring,"start")){ 		//开始更新固件
#ifdef DOWN_IMAGE		
			Create_PlaySystemEventVoices(UPDATA_START_PLAY);
		}else if(!strcmp(pSub->valuestring,"error")){	//更新固件错误
			Create_PlaySystemEventVoices(UPDATA_ERROR_PLAY);
#endif			
		}else if(!strcmp(pSub->valuestring,"end")){ 	//更新固件结束
			Create_PlaySystemEventVoices(UPDATA_END_PLAY);
		}
	}//  end updateImage                // end----------->由版本监测进程发送过来
	else if(!strcmp(pSub->valuestring,"newImage")){	// app端确认还是取消更新操作
		pSub = cJSON_GetObjectItem(pJson, "status");
		if(!strcmp(pSub->valuestring,"ok")){		//app 确认更新
			
		}else if(!strcmp(pSub->valuestring,"miss")){//app 取消更新

		}
	}else if(!strcmp(pSub->valuestring,"ServerWifi")){
		int event = cJSON_GetObjectItem(pJson, "event")->valueint;
		Create_PlaySystemEventVoices(event);
		if(CONNECT_OK==event){
			//Link_NetworkOk();
		}else if(NOT_NETWORK==event){
			//Link_NetworkError();
		}
	}else if(!strcmp(pSub->valuestring,"TestNet")){
		test_brocastCtr(sockfd,peer,recvdata);
	}else if(!strcmp(pSub->valuestring,"qtts")){
#if defined(HUASHANG_JIAOYU)	
		if(!strncmp(cJSON_GetObjectItem(pJson, "text")->valuestring,"123456",6)){
			Huashang_changePlayVoicesName();
		}
#endif	
		if(!strncmp(cJSON_GetObjectItem(pJson, "text")->valuestring,"openwifi",8)){
			OpenWifi();
		}
#if defined(HUASHANG_JIAOYU)	
		Create_PlaySystemEventVoices(KEY_DOWN_PLAY);
		char *WeiXintxt =cJSON_GetObjectItem(pJson, "text")->valuestring;
		AddWeiXinMessage_Text((const char *)WeiXintxt,strlen(WeiXintxt));
#else
		Create_PlayQttsEvent(cJSON_GetObjectItem(pJson, "text")->valuestring,QTTS_UTF8);
#endif
	}
	else if (!strcmp(pSub->valuestring,"speek")){//微信对讲
#if defined(HUASHANG_JIAOYU)
		Create_PlaySystemEventVoices(KEY_DOWN_PLAY);
		char *WeiXinFile =cJSON_GetObjectItem(pJson, "file")->valuestring;
		AddWeiXinMessage_Voices((const char *)WeiXinFile,strlen(WeiXinFile));
#else
		CreatePlayWeixinVoicesSpeekEvent((const char *)cJSON_GetObjectItem(pJson, "file")->valuestring);
#endif
	}
	else if (!strcmp(pSub->valuestring,"binddev")){
		pSub = cJSON_GetObjectItem(pJson, "status");
		if(!strcmp(pSub->valuestring,"failed")){
			Create_PlayQttsEvent(TULING_PLAY_TEXT_WEIXIN_FAILED,QTTS_GBK);
		}else if(!strcmp(pSub->valuestring,"ask")){
			EnableBindDev();
			Create_PlaySystemEventVoices(BIND_SSID_PLAY);
		}
	}
	else if (!strcmp(pSub->valuestring,"call")){		//微信界面发送过来的呼叫请求
#if defined(QITUTU_SHI)	
		EnableCallDev();
		Create_PlaySystemEventVoices(TALK_CONFIRM_PLAY);
#endif		
	}
	else if (!strcmp(pSub->valuestring,"uploadfile")){
		pSub = cJSON_GetObjectItem(pJson, "status");
		if(!strcmp(pSub->valuestring,"ok")){			//发送成功
		}else if(!strcmp(pSub->valuestring,"failed")){	//发送失败
		}else if(!strcmp(pSub->valuestring,"timeout")){	//发送失败
			Create_PlaySystemEventVoices(SEND_LINK_ER_PLAY);//"当前网络环境差，语音发送失败，请检查网络。"
		}
	}
	else if (!strcmp(pSub->valuestring,"clock")){
		pSub = cJSON_GetObjectItem(pJson, "status");
		if(!strcmp(pSub->valuestring,"ok")){			//到点报语音
			//报语音
			char *path=NULL;
			if(cJSON_GetObjectItem(pJson, "path")!=NULL){
				path=cJSON_GetObjectItem(pJson, "path")->valuestring;
			}
			CreatePlayWeixinVoicesSpeekEvent(path);
		}else if(!strcmp(pSub->valuestring,"close")){	//关闭设置开机时间
			//
			char *time_open=NULL;
			if(cJSON_GetObjectItem(pJson, "time")!=NULL){
				time_open=cJSON_GetObjectItem(pJson, "time")->valuestring;
			}
			printf("clock close (%s)...\n",time_open);
			usleep(500*1000);
			SocSendMenu(3,0);
			usleep(100*1000);
			test_Clock_saveLoute((const char *)time_open);
			SocSendMenu(7,time_open);	//设置闹钟开机时间
	
		}
	}else if(!strcmp(pSub->valuestring,"getdev")){	//微信获取设备信息
		ack_alluserCtr(sockfd,Get_batteryVaule(),get_dc_state());
	}
	else if (!strcmp(pSub->valuestring,"tuling")){
		Write_tulinglog("recv tuling val\n");
		pSub = cJSON_GetObjectItem(pJson, "userId");
		if(pSub){
			char *userId= pSub->valuestring;
			char *token= cJSON_GetObjectItem(pJson, "token")->valuestring;
			Load_useridAndToken((const char *) userId,(const char *) token);
		}
	}else if(!strcmp(pSub->valuestring,"downmp3")){	//微信端下载歌曲事件，已经下载到 /Down/ 目录下
		char *status= cJSON_GetObjectItem(pJson, "status")->valuestring;
		if(!strcmp(status,"ok")){	//已经下载完
			char *cacheMp3file= cJSON_GetObjectItem(pJson, "mp3file")->valuestring;
#ifdef PALY_URL_SD			
			Create_SaveWeixinDownMp3_EventToMainQueue(cacheMp3file);
#endif
		}else{
		}
	}
#if defined(HUASHANG_JIAOYU)	//华上语音识别接口，识别出来的结果
	else if(!strcmp(pSub->valuestring,"localMp3")){
		WeiXin_playhuaShangMusic(cJSON_GetObjectItem(pJson, "nums")->valueint);
	}
#endif	
exit:
	cJSON_Delete(pJson);
}
