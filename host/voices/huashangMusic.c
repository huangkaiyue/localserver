#include "comshead.h"
#include "systools.h"
#include "config.h"
#include "host/voices/callvoices.h"
#include "../sdcard/musicList.h"
#include "base/cJSON.h"
#include "gpio_7620.h"

#ifdef HUASHANG_JIAOYU		//获取华上教育sdcard当中国学的歌曲内容

//------------------------------------------------------------------------------
#define	HUASHANG_MUSIC_TOTAL_NUM 	3819
static int PlayHuashang_MusicIndex=0;	//播放华上教育歌曲下表编号 
static int Huashang_MusicTotal=HUASHANG_MUSIC_TOTAL_NUM;		
//开机获取华上教育内容播放记录
void openSystemload_huashangData(void){
	char jsonfile[128]={0};
	snprintf(jsonfile,128,"%s%s",TF_SYS_PATH,HUASHANG_JIAOYU_PLAY_JSON_FILE);
	char *filebuf = readFileBuf(jsonfile);
	if(filebuf==NULL){
		Write_huashang_log((const char *)"openSystemload_huashangData",(const char * )"read filefailed",1);
		return ;
	}
	cJSON * pJson = cJSON_Parse(filebuf);
	if(NULL == pJson){
		goto exit0;
	}
	cJSON * pSub = cJSON_GetObjectItem(pJson, "playindex");
	if(pSub==NULL){
		goto exit0;
	}
	PlayHuashang_MusicIndex = pSub->valueint;
	pSub = cJSON_GetObjectItem(pJson, "total");
	if(pSub==NULL){
		goto exit0;
	}
	Huashang_MusicTotal=pSub->valueint;
exit0:	
	free(filebuf);
}
int GetScard_forPlayHuashang_Music(unsigned char playMode,const void *playDir){
	int ret=-1;
	char playBuf[128]={0};
	if(access(TF_SYS_PATH, F_OK)){	//检查tf卡
		if(GetRecordeVoices_PthreadState()==RECODE_PAUSE)
			Create_PlaySystemEventVoices(TF_ERROT_PLAY);
		return ret;
	}
	if(playMode==PLAY_NEXT){
		if(++PlayHuashang_MusicIndex>HUASHANG_MUSIC_TOTAL_NUM){
			PlayHuashang_MusicIndex=0;
		}
		
	}else if(playMode==PLAY_PREV){
		if(--PlayHuashang_MusicIndex>HUASHANG_MUSIC_TOTAL_NUM){
			PlayHuashang_MusicIndex=HUASHANG_MUSIC_TOTAL_NUM;
		}
	}	
	snprintf(playBuf,128,"%s%s/%d.mp3",TF_SYS_PATH,HUASHANG_GUOXUE_DIR,PlayHuashang_MusicIndex);
	if(access(playBuf, F_OK)==0){
		Write_huashang_log((const char *)"get play file ok",(const char * )playBuf,2);
		ret=__AddLocalMp3ForPaly((const char *)playBuf);
	}else{
		Write_huashang_log((const char *)"get play file failed",(const char * )playBuf,3);
	}	
	return ret;
}
//关机保存华上教育内容播放记录数据
void closeSystemSave_huashangData(void){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	pItem = cJSON_CreateObject();
	cJSON_AddNumberToObject(pItem, "playindex", PlayHuashang_MusicIndex);
	cJSON_AddNumberToObject(pItem, "total",Huashang_MusicTotal); 
	szJSON = cJSON_Print(pItem);
	cJSON_Delete(pItem);
	free(szJSON);
}

/***
播放列表歌曲，按键控制单曲还是列表播放 
data:播放的数据
musicType:音乐类型  网络歌曲/本地歌曲

***/
void CreatePlayListMuisc(const void *data,int musicType){
	if(PLAY_MUSIC_NETWORK==musicType){
		__AddNetWork_UrlForPaly(data);
	}else if(PLAY_MUSIC_SDCARD==musicType){
		__AddLocalMp3ForPaly((const char *)data);
	}
}
//------------------------------------------------------------------------------

//华上教育按键按下播放按键
void Huashang_keyDown_playkeyVoices(int state){
	if(state==GPIO_UP){
	}else {

	}
}
static unsigned char AifiState=0;
#define UNKOWN_AIFI_STATE	0
#define	TULING_AIFI_OK		1
#define	XUNFEI_AIFI_OK		2


void SetAifi_voicesState(unsigned char aifiState){
	AifiState=aifiState;
}
int GetAifi_voicesState(void){
	return (int)AifiState;
}

//发送离线语音识别请求给 网络服务器，进行语音识别
void Huashang_SendnotOnline_xunfeiVoices(const char *filename){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	pItem = cJSON_CreateObject();
	cJSON_AddStringToObject(pItem, "handler", "xunfei");
	cJSON_AddStringToObject(pItem, "filename",filename); 
	szJSON = cJSON_Print(pItem);
	int ret= SendtoServicesWifi(szJSON,strlen(szJSON));
	cJSON_Delete(pItem);
	free(szJSON);
}
//获取到讯飞识别出来的aifi 语音结果
void GetHua_xunfei_aifiVoices(const char *xunfeiAifi){
	char* szJSON = NULL;
	cJSON* pItem = NULL;
	unsigned char aifiState=0;
	char *data = (char *)calloc(1,strlen(xunfeiAifi)+1);
	if(data==NULL){
		goto exit;
	}
	sprintf(data,"%s",szJSON);
	
exit:
	SetAifi_voicesState(UNKOWN_AIFI_STATE);
}

#endif

