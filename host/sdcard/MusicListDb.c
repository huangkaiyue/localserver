#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "musicList.h"

#define DATABASE_NAME	"music.db"  	//���ݿ���
#define MESSAGE_TABLE	"message"	//��¼������Ϣ����	

typedef struct{
	List_t list[MUSIC_LIST];
}MusicList_t;

MusicList_t *Mlist=NULL;

//���ݿ��:�������ID�ź����� (���������б���)
static void CreateMusicTable(const char *tableMusic){
	char sql[128]={0};
	snprintf(sql,128,"create table if not exists %s (id INTEGER PRIMARY KEY,name VARCHAR(64) not null unique);",tableMusic);            
	CreateTable(sql);
}
//��������б���Ϣ,���沥����ʷ��¼ֵ
static void CreateMusicListMesage(const char *tableMusic){
	char sql[128]={0};
	snprintf(sql,128,"create table if not exists %s (name VARCHAR(64) not null unique,number INTEGER,DirTime INTEGER);",tableMusic);            
	CreateTable(sql);
}

static void checkpath(const char *path,char *getPath){
	int len = strlen(path);
	if(*(path+len-1)=='/'){
		strncat(getPath,path,len);
	}else{
		strncat(getPath,path,len);
		strncat(getPath,"/",1);
	}
}
//����Ŀ¼����ȡ������ȥ������� / ����
static void GetTableName(const char *path,char *destPath){
	int len = strlen(path);
	if(*(path+len-1)=='/'){
		strncat(destPath,path,len-1);
	}else{
		strncat(destPath,path,len);
	}
}
static int onloadSdcardFile(const char *sdcard,List_t *list){
	DIR *dirptr = NULL;  
	char getPath[128]={0};
	int fileNum=0;
	checkpath(sdcard,getPath);
	strcat(getPath,list->listname);
	
	struct stat buf;
	if(stat(getPath, &buf)){
		return -1;
	}
	
	int mtime = (int)buf.st_mtime;
	if(mtime==list->DirTime){
		printf("%s dir  data is new \n",list->listname);
		return -1;
	}
	list->DirTime = mtime;//�������ݿ⵱��Ŀ¼����ʱ��
	struct dirent *entry;  
	if((dirptr = opendir(getPath)) == NULL)  {  
		perror("open dir failed!\n"); 
		return -1;  
	}

	while (entry = readdir(dirptr)){  
		if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,"..")){
			continue;
		}
		printf("list->listname = %s\n",list->listname);
		InsertSql(list->listname,entry->d_name);
		fileNum++;			
	}
	list->Nums =fileNum;
	closedir(dirptr);   
	return 0;
}
static enum{
	mp3_N=1,
	story_N,
	english_N,
	guoxue_N,
};

void SaveSystemPlayNum(void){
#if 0	
	set_paly_num(Mlist->list[0].playindex,mp3_N);
	usleep(100);
	set_paly_num(Mlist->list[1].playindex,story_N);
	usleep(100);
	set_paly_num(Mlist->list[2].playindex,english_N);
	usleep(100);
	set_paly_num(Mlist->list[3].playindex,guoxue_N);
	usleep(100);
#else
	List_t cachelist;
	int i=0;
	for(i=0;i<4;i++){
		snprintf(cachelist.listname,24,"%s%s","cache",Mlist->list[i].listname);
		if(InsertMusicMessageSQL(MESSAGE_TABLE,cachelist.listname,Mlist->list[i].playindex,0)){
			UpdateSqlByMessage(MESSAGE_TABLE,cachelist.listname,Mlist->list[i].playindex,0);
		}
		printf("Mlist->list[%d].playindex =%d\n",i,Mlist->list[i].playindex);
	}
#endif
}
/*
������ȡĿ¼�ļ������ص����ݿ⵱��
*/
int SysOnloadMusicList(const char *sdcard,const char *mp3Music,const char *story,const char *english,const char *guoxue){
	char sqlPath[128]={0};
	
	checkpath(sdcard,sqlPath);
	strcat(sqlPath,DATABASE_NAME);
	if(OpenSql(sqlPath) != 0)
		return -1;

	int i=0,listNum=MUSIC_LIST;
	GetTableName(mp3Music,Mlist->list[0].listname);
	GetTableName(story,Mlist->list[1].listname);
	GetTableName(english,Mlist->list[2].listname);
	GetTableName(guoxue,Mlist->list[3].listname);

#if 0	
	get_paly_num(&Mlist->list[0].playindex,mp3_N);
	get_paly_num(&Mlist->list[1].playindex,story_N);
	get_paly_num(&Mlist->list[2].playindex,english_N);
	get_paly_num(&Mlist->list[3].playindex,guoxue_N);
#else
	List_t cachelist;
	for(i=0;i<4;i++){
		// ����Ŀ¼���е� cachemp3 �ֶ����棬Nums��¼���ǹػ�֮�󣬲��ŵ�num���
		memset(&cachelist,0,sizeof(List_t));
		snprintf(cachelist.listname,24,"%s%s","cache",Mlist->list[i].listname);
		GetMusicMessageSQL(MESSAGE_TABLE,&cachelist);
		Mlist->list[i].playindex=cachelist.Nums;
		printf("Mlist->list[%d].playindex =%d\n",i,Mlist->list[i].playindex);
	}
#endif

#ifdef DOWN_URL_MUSIC	
	CreateMusicTable(XIMALA_MUSIC);
	sprintf(Mlist->list[MUSIC_LIST-1].listname,"%s",XIMALA_MUSIC);
	GetMusicMessageSQL(MESSAGE_TABLE,&(Mlist->list[MUSIC_LIST-1]));
	printf("Mlist->list[MUSIC_LIST-1].Nums:%d Mlist->list[i].listname:%s\n",Mlist->list[MUSIC_LIST-1].Nums,Mlist->list[MUSIC_LIST-1].listname);
	listNum-=1;
#endif	
	CreateMusicListMesage(MESSAGE_TABLE);
	for(i=0;i<listNum;i++){
		GetMusicMessageSQL(MESSAGE_TABLE,&(Mlist->list[i]));
		CreateMusicTable(Mlist->list[i].listname);
		//printf("Mlist->list[0].listname =%s\n",Mlist->list[i].listname);
		if(!onloadSdcardFile(sdcard,&(Mlist->list[i]))){
			if(InsertMusicMessageSQL(MESSAGE_TABLE,Mlist->list[i].listname,Mlist->list[i].Nums,Mlist->list[i].DirTime)){
				UpdateSqlByMessage(MESSAGE_TABLE,Mlist->list[i].listname,Mlist->list[i].Nums,Mlist->list[i].DirTime);
			}
		}
	}
	return 0;
}

/*

*/
int GetSdcardMusic(const char *sdcard,const char *musicDir,char *getMusicname,unsigned char Mode){
	int i=0;
	for(i=0;i<MUSIC_LIST;i++){
		if(!strncmp(Mlist->list[i].listname,musicDir,strlen(Mlist->list[i].listname))){
			break;
		}
		//printf("musicDir = %s Mlist->list[i].listname=%s\n",musicDir,Mlist->list[i].listname);
	}
	if(i>=MUSIC_LIST){
		printf("warning is GetSdcardMusic \n");
		return -1;
	}
	switch(Mode){
		case PLAY_NEXT:
			++Mlist->list[i].playindex;
			if(Mlist->list[i].playindex>Mlist->list[i].Nums){
				Mlist->list[i].playindex=1;//���ݿ⵱�е�ID �Ǵ�1��ʼ
			}
			break;
		case PLAY_PREV:
			--Mlist->list[i].playindex;
			if(Mlist->list[i].playindex<=0){
				Mlist->list[i].playindex=Mlist->list[i].Nums;
			}
			break;
		default:
			return -1;
	}
	printf("GetSdcardMusic : playindex=%d (%d)\n",Mlist->list[i].playindex,Mlist->list[i].Nums);
	return GetTableSqlById(Mlist->list[i].listname,Mlist->list[i].playindex,getMusicname);
}
#ifdef DOWN_URL_MUSIC
int InsertXimalayaMusic(const char *musicDir,const char *musicName){
//	printf("InsertXimalayaMusic musicDir=%s musicName=%s \n",musicDir,musicName);
	if(InsertSql(musicDir,musicName))
		return -1;
//	printf("InsertXimalayaMusic ok \n");
	Mlist->list[MUSIC_LIST-1].Nums++;
	if(InsertMusicMessageSQL(MESSAGE_TABLE,Mlist->list[MUSIC_LIST-1].listname,Mlist->list[MUSIC_LIST-1].Nums,0)){
//			printf("Mlist->list[MUSIC_LIST-1].listname =%s Mlist->list[MUSIC_LIST-1].Nums=%d\n",Mlist->list[MUSIC_LIST-1].listname,Mlist->list[MUSIC_LIST-1].Nums);
			UpdateSqlByMessage(MESSAGE_TABLE,Mlist->list[MUSIC_LIST-1].listname,Mlist->list[MUSIC_LIST-1].Nums,0);
	}
	return 0;
}
int DelXimalayaMusic(const char *musicDir,const char *musicName){
	if(del_DBdata(musicDir,musicName))
		return -1;
	Mlist->list[MUSIC_LIST-1].Nums--;
	if(InsertMusicMessageSQL(MESSAGE_TABLE,Mlist->list[MUSIC_LIST-1].listname,Mlist->list[MUSIC_LIST-1].Nums,0)){
			UpdateSqlByMessage(MESSAGE_TABLE,Mlist->list[MUSIC_LIST-1].listname,Mlist->list[MUSIC_LIST-1].Nums,0);
	}
	return 0;
}
#endif
int InitMusicList(void){
	Mlist = (MusicList_t *)calloc(1,sizeof(MusicList_t));
	if(Mlist==NULL){
		return -1;
	}
	return 0;
}
void CleanMusicList(void){
	if(Mlist){
		free(Mlist);
		Mlist=NULL;
	}
	CloseSql();
}

int checkMusicDb(const char *sdcard,const char *musicDir){

}
//#define MAIN_TEST
	
#ifdef MAIN_TEST
void testPlayList(void){
	char musicname[128]={0};
	int i=0;
	for(i=0;i<7;i++)	{
		//GetSdcardMusic((const char *)"sdcard",(const char *)"keji",musicname,PLAY_NEXT);
		GetSdcardMusic((const char *)"sdcard/",(const char *)"why/",musicname,PLAY_PREV);
		printf("musicname[%d] = %s\n",i,musicname);
		memset(musicname,0,128);
	}
}
void test_GetlocalSdcard(void){
	char getMusicname[128]={0};
	int i=0;
	for(i=0;i<12;i++)	{
		GetSdcardMusic((const char *)"sdcard/",(const char *)XIMALA_MUSIC,getMusicname,PLAY_NEXT);
		printf("XIMALA_MUSIC[%d] = %s\n",i,getMusicname);
		memset(getMusicname,0,128);
	}
}
int main(void){
	InitMusicList();
	SysOnloadMusicList((const char *)"sdcard/",(const char *)"keji/",(const char *)"why/",(const char *)"english/",(const char *)"guoxue/");
	testPlayList();
	InsertXimalayaMusic((const char *)XIMALA_MUSIC,(const char *)"testmusic11");
	InsertXimalayaMusic((const char *)XIMALA_MUSIC,(const char *)"testmusic12");
	InsertXimalayaMusic((const char *)XIMALA_MUSIC,(const char *)"testmusic13");
	SaveSystemPlayNum();
	test_GetlocalSdcard();
	CleanMusicList();
	return 0;
}
#endif