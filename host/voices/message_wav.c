#include "comshead.h"
#include "host/voices/wm8960i2s.h"
#include "host/voices/callvoices.h"
#include "host/studyvoices/prompt_tone.h"
#include "host/voices/WavAmrCon.h"
#include "config.h"

//------------------------------------------------------
static unsigned short playNetwork_pos=0;
static unsigned char  playNetworkState=0;

void SetPlayNetworkState(unsigned char state){
	playNetworkState = state;
}
int safeSetPlayNetworkState(unsigned char state){
	if(playNetworkState==INTERRUPT_PLAY_WAV){
		return -1;	
	}
	playNetworkState=state;
	return 0;
}
int GetPlayNetworkState(void){
	return (int)playNetworkState;
}
void ExitPlayNetworkState(void){
	SetPlayNetworkState(INTERRUPT_PLAY_WAV);
	ExitPlayNetworkPlay();
}
int WriteStreamPcmData(char *data,int len){
	int i=0;
	for(i=0;i<len;i+=2){
		memcpy(play_buf+playNetwork_pos,data+i,2);
		playNetwork_pos += 2;
		memcpy(play_buf+playNetwork_pos,data+i,2);
		playNetwork_pos += 2;

		if(playNetwork_pos==I2S_PAGE_SIZE){
			write_pcm(play_buf);
			playNetwork_pos=0;
		}
		if(playNetworkState==INTERRUPT_PLAY_WAV){
			playNetwork_pos=0;
			return -1;
		}
#if 0		
		if(playNetwork_pos+4>I2S_PAGE_SIZE){
			printf(".......................error write data ................\n");
			playNetwork_pos=0;
			continue;
		}
#endif		

	}
	return 0;
}
//------------------------------------------------------
static unsigned char playWavState=0;
void SetPlayWavState(unsigned char state){
	playWavState=state;
}
int SafeSetPlayWavState(unsigned char state){
	if(playWavState==INTERRUPT_PLAY_WAV){
		return -1;	
	}
	playWavState=state;
	return 0;
}
int GetPlayWavState(void){
	return (int)playWavState;
}
void ExitPlay_WavVoices(void){
	SetPlayWavState(INTERRUPT_PLAY_WAV);
}
//------------------------------------------------------
void WritePcmData(char *data,int size){
	if(I2S.play_size==I2S_PAGE_SIZE){
		I2S.play_size=0;
		write_pcm(play_buf);
	}
	memcpy(play_buf+I2S.play_size,data,size);
	I2S.play_size +=size;
}
//播放单声道wav格式音频数据
static void PlaySignleWavVoices(const char *playfilename,unsigned char playMode){
	int r_size=0,pos=0;
	char readbuf[2]={0};
	FILE *fp= fopen(playfilename,"r");
	if(fp==NULL){
		printf("open sys failed \n");
		return ;
	}
	SetWm8960Rate(RECODE_RATE);
	if(playMode==PLAY_IS_INTERRUPT&&SafeSetPlayWavState(START_PLAY_WAV)){
		printf("set play wav state failed ; current is interrupt play system wav voices \n");
		goto exit;
	}
	fseek(fp,WAV_HEAD,SEEK_SET);		//跳过wav头部	
	while(1){
		r_size= fread(readbuf,1,2,fp);
		if(r_size==0){
			if(pos>0){
				memset(play_buf+pos,0,I2S_PAGE_SIZE-pos);		//清空上一次尾部杂音,并播放尾音
				write_pcm(play_buf);
			}
			pause_record_audio();
			CleanI2S_PlayCachedata();
			break;
		}
		memcpy(play_buf+pos,readbuf,2);
		pos+=2;
		memcpy(play_buf+pos,readbuf,2);
		pos+=2;
		if(pos==I2S_PAGE_SIZE){
			write_pcm(play_buf);
			pos=0;
		}
		if(GetPlayWavState()==INTERRUPT_PLAY_WAV&&playMode==PLAY_IS_INTERRUPT){
			pause_record_audio();
			CleanI2S_PlayCachedata();//清理
			StopplayI2s();			 //最后一片数据丢掉
			break;
		}	
	}
	fclose(fp);
	memset(play_buf,0,I2S_PAGE_SIZE);
	SetPlayWavState(START_PLAY_WAV);
	return ;
exit:
	pause_record_audio();
	SetPlayWavState(START_PLAY_WAV);
}
//播放单声道amr格式音频数据
static void playAmrVoices(const char *filename,unsigned char playMode){
	char *outfile ="speek.wav";
	AmrToWav8k(filename,(const char *)outfile);
	PlaySignleWavVoices((const char *)outfile,playMode);
	remove(outfile);
}
static void __playAmrVoices(const char *filePath,unsigned char playMode){
	char path[128]={0};
	snprintf(path,128,"%s%s",sysMes.localVoicesPath,filePath);
	playAmrVoices(path,playMode);
}

#ifdef SPEEK_VOICES
/********************************************************
@ 播放接收到手机发送的对讲消息
@ filename:缓存到本地的wav数据的文件路径 (播放完需要删除)
@
*********************************************************/
void playspeekVoices(const char *filename){
	playAmrVoices(filename,PLAY_IS_INTERRUPT);
	remove(filename);
}
#endif
/********************************************************
@ 函数功能:	播放系统音
@ filePath:	路径
@ 返回值: 无
*********************************************************/
void PlaySystemAmrVoices(const char *filePath){
	__playAmrVoices(filePath,PLAY_IS_INTERRUPT);
}
//播放过渡音，不允许打断
void play_waitVoices(const char *filePath){
	__playAmrVoices(filePath,PLAY_IS_COMPLETE);
}
//检查播放网路下载音频文件尾音
static int checkPlayNetwork_endVoices(void){
	int ret=0;
	if(playNetworkState==INTERRUPT_PLAY_WAV){
		Mute_voices(MUTE);
		pause_record_audio();		//退出播放状态
		CleanI2S_PlayCachedata();	//清理
		StopplayI2s();				//最后一片数据丢掉
		memset(play_buf,0,I2S_PAGE_SIZE);
		ret=-1;
	}else{
		if(playNetwork_pos!=0){		//播放尾音
			memset(play_buf+playNetwork_pos,0,I2S_PAGE_SIZE-playNetwork_pos);
			write_pcm(play_buf);
		}
		memset(play_buf,0,I2S_PAGE_SIZE);
		pause_record_audio();		//退出播放状态
		CleanI2S_PlayCachedata();	//清理
	}
	playNetworkState =0;	
	SetPlayNetworkState(START_PLAY_WAV);
	SetplayNetwork_unLock();
	return ret;
}
/********************************************************
@ 函数功能:	播放QTTS数据
@ text:文本		type:文本类型
@ 返回值: 无
*********************************************************/
void PlayQttsText(char *text,unsigned char type){
	SetWm8960Rate(RECODE_RATE);
	char *textbuf= (char *)calloc(1,strlen(text)+2);
	if(textbuf==NULL){
		perror("calloc error !!!");
		goto exit;
	}
	if(safeSetPlayNetworkState(START_PLAY_WAV)){
		SetPlayNetworkState(START_PLAY_WAV);
		goto exit;
	}
	sprintf(textbuf,"%s%s",text,",");	//文本尾部添加",",保证文本播报出来
	PlayQtts_log("play qtts start\n");
	printf("start play qtts \n");
	Qtts_voices_text(textbuf,type);
	free(textbuf);
	checkPlayNetwork_endVoices();
	return ;
exit:
	SetplayNetwork_unLock();
	pause_record_audio();
}
/********************************************************
@ 函数功能:	播放图灵数据
@ url:图灵发送
@ 返回值: 0 正常退出 -1非正常退出
*********************************************************/
int PlayTulingText(const char *url){
	if(safeSetPlayNetworkState(START_PLAY_WAV)){
		SetPlayNetworkState(START_PLAY_WAV);
		SetplayNetwork_unLock();
		return -1;
	}
	SetWm8960Rate(RECODE_RATE); 
	downTulingMp3((const char*)url);
	return checkPlayNetwork_endVoices();
}	
