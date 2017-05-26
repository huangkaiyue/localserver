#include "comshead.h"
#include "qtts_qisc.h"
#include "host/voices/wm8960i2s.h"
#include "host/voices/callvoices.h"
#include "host/studyvoices/prompt_tone.h"

#define CMD_UNKOWN					-1	//未知命令
#define CMD_MUSIC_MEUN				0	//点播音乐
#define CMD_WHO_NAME				1	//打断关键数据播放
#define CMD_ADD_VOL					2	//发现重要的text
#define CMD_SUB_VOL					3	//发现重要的text


/********************************************************
@鍑芥暟鍔熻兘:	鍖归厤鏂囧瓧璇煶鎺у埗
@鍙傛暟:	text 鍖归厤鐨勬枃鏈�
@杩斿洖:	0琛ㄧず鍖归厤鎴愬姛
@	  	-1琛ㄧず鍖归厤澶辫触
*********************************************************/
int CheckinfoText_forContorl(const char *text,char *getPlayMusicName){
	int ret = CMD_UNKOWN;
	if(strstr(text,"鍚嶅瓧")||strstr(text,"浣犳槸璋�")){	
		ret =CMD_WHO_NAME;
	}
	else if(strstr(text,"闊抽噺")){
		if((strstr(text,"鍔�")&&strstr(text,"鍑�"))||(strstr(text,"澶�")&&strstr(text,"灏�")))
			ret =CMD_UNKOWN;
		else if(strstr(text,"鍔�")||strstr(text,"澶�")){	
			ret =CMD_ADD_VOL;
		}
		else if(strstr(text,"鍑�")||strstr(text,"灏�")){	
			ret =CMD_SUB_VOL;
		}
	}else if(strstr(text,"鎾斁")){
		Write_huashangTextLog(text);
		if(Huashang_Checkutf8(text,getPlayMusicName)==0){
			ret =CMD_MUSIC_MEUN;
		}
	}
	return ret;
}

int HandlerPlay_checkTextResult(int cmd,const char *playname,unsigned int playEventNums){
	int ret=-1;
	switch(cmd){
		case CMD_MUSIC_MEUN:
			pause_record_audio();//需要切换到暂停状态，才能添加歌曲进去播放------------>当前状态为播放wav状态
			Write_huashangTextLog(playname);
			ret =__AddLocalMp3ForPaly(playname,EXTERN_PLAY_EVENT);		
			break;
		case CMD_WHO_NAME:
			ret =PlaySystemAmrVoices(TULING_HAHAXIONG,playEventNums);
			break;
		case CMD_ADD_VOL:
			Setwm8960Vol(VOL_ADD,0);
			ret =PlaySystemAmrVoices(VOICE_ADD,playEventNums);
			ack_VolCtr("add",GetVol());//----------->闊抽噺鍑�			
			break;
		case CMD_SUB_VOL:
			Setwm8960Vol(VOL_SUB,0);
			ret =PlaySystemAmrVoices(VOICE_SUB,playEventNums);
			ack_VolCtr("sub",GetVol());//----------->闊抽噺鍑�
			break;
	}
	return ret;
}
