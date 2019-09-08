#include "AM.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bf.h"
#include "defn.h"
#include "functions.h"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      AM_errno = AME_BF_ERR;  \
      return AM_errno;        \
    }                         \
  }

static int f_index;			//index of open files array.
static int s_index;			//index of open scans array.
static int count_o;			//count of open files.
static int count_s;			//count of open scans


int AM_errno = AME_OK;

void AM_Init() {

	BF_Init(LRU);

	/*OpenFileAr = malloc(MAXOPENFILES);
	OpenScanAr = malloc(MAXSCANS);	//Allocate memory for arrays.*/

	f_index = 0;
	s_index = 0;			//Initialize indexes.
	count_o = 0;			//Initialize file counter.
	count_s = 0;			//Initialize scan counter.
	int i;
	for(i=0; i<MAXOPENFILES; i++){
		OpenFileAr[i].filedesc = -1;
		OpenFileAr[i].filename = NULL;
		OpenScanAr[i].filedesc = -1;
		//OpenScanAr[i].value = malloc(40);
		//OpenScanAr[i].end_v = malloc(40);
		OpenScanAr[i].value = NULL;
		OpenScanAr[i].end_v = NULL;
	}				//Initialize arrays.

	return;
}


int AM_CreateIndex(char *fileName,
	               char attrType1,
	               int attrLength1,
	               char attrType2,
	               int attrLength2) {

	//checking if attributes, length are ok.
	if(Match(attrType1, attrLength1) == 0 || Match(attrType2, attrLength2) == 0){
		AM_errno = AME_WRONG_ATTR;
		return AM_errno;
	}

	if(count_o > MAXOPENFILES){
		AM_errno = AME_MAX_OPEN_FILES;
		return AM_errno;
	}

	BF_Block *block;
	BF_Block_Init(&block);
	int fd, j=0;

	CALL_OR_DIE(BF_CreateFile(fileName));				//Create file
	CALL_OR_DIE(BF_OpenFile(fileName, &fd));			//Open file
	CALL_OR_DIE(BF_AllocateBlock(fd, block));
	char c = 'A'; char *data; char end = '\0';

	data = BF_Block_GetData(block);
	memcpy(&data[j], &c, sizeof(c));				//File identifier.
	j += sizeof(c);

	memcpy(&data[j], &attrType1, sizeof(attrType1));		//Type1
	j += sizeof(attrType1);

	memcpy(&data[j], &attrLength1, sizeof(attrLength1));		//Length1
	j += sizeof(attrLength1);

	memcpy(&data[j], &attrType2, sizeof(attrType2));		//Type2
	j += sizeof(attrType2);

	memcpy(&data[j], &attrLength2, sizeof(attrLength2));		//Length2
	j += sizeof(attrLength2);

	/*memcpy(&data[j], &fd, sizeof(fd));				//FileDescriptor
	j += sizeof(fd);*/


	BF_Block_SetDirty(block);
	CALL_OR_DIE(BF_UnpinBlock(block));
	CALL_OR_DIE(BF_CloseFile(fd));					//Close file.
	BF_Block_Destroy(&block);

	return AME_OK;
}


int AM_DestroyIndex(char *fileName) {

	int i;
	for(i=0; i<MAXOPENFILES; i++){
		if(strcmp(OpenFileAr[i].filename, fileName) == 0){
			AM_errno = AME_OPEN_FILE;
			return AM_errno;
		}
	}

	char *s;
	s = malloc(20);
	sprintf(s, "rm %s", fileName);
	system(s);				//delete file with fileName.
	free(s);
	return AME_OK;
}


int AM_OpenIndex (char *fileName) {

	BF_Block *block;
	BF_Block_Init(&block);
	int fd;

	if(count_o > MAXOPENFILES){
		AM_errno = AME_MAX_OPEN_FILES;
		return AM_errno;
	}

	CALL_OR_DIE(BF_OpenFile(fileName, &fd));
	CALL_OR_DIE(BF_GetBlock(fd, 0, block));				//Get block 0, check if it is AM file.
	char *data; char c;
	int i;
	data = BF_Block_GetData(block);

	if((c=data[0]) != 'A'){
		fprintf(stderr, "This is not an AM file!\n");
		AM_errno = AME_NOT_AMFILE;
		return AM_errno;
	}								//Check if AM file.

	//write metadata to OpenFile array
	for(i=0; i<MAXOPENFILES; i++){
		if(OpenFileAr[i].filedesc == -1){
			f_index = i;
			break;					//fix f_index to point to th 1st open position of the array.
		}
	}
	int j = sizeof(c);

	OpenFileAr[f_index].v1_type = data[j];			//type of value1
	j++;

	OpenFileAr[f_index].v1_size = data[j];			//length of value1
	j += 4;

	OpenFileAr[f_index].v2_type = data[j];			//type of value2
	j++;

	OpenFileAr[f_index].v2_size = data[j];			//length of value2
	j += 4;

	//OpenFileAr[f_index].filedesc = data[j];			//file descriptor
	OpenFileAr[f_index].filedesc = fd;

	OpenFileAr[f_index].filename = malloc(strlen(fileName)+1);
	strcpy(OpenFileAr[f_index].filename, fileName);		//filename

	count_o++;

	CALL_OR_DIE(BF_UnpinBlock(block));
	return f_index;
}


int AM_CloseIndex (int fileDesc) {

	if(fileDesc >= MAXOPENFILES || fileDesc < 0){
		printf("INDEX = %d\n", fileDesc);
		AM_errno = AME_WRONG_INDEX;
		return AM_errno;
	}
	int fd, j, i;

	//take file descriptor of file in array position of fileDesc
	fd = OpenFileAr[fileDesc].filedesc;
	//printf("\tClose Index: fd, %d\n", fd);

	//check if open scans
	for(j=0; j<MAXSCANS; j++){
		if(OpenScanAr[j].filedesc == fd){
			AM_errno = AME_OPEN_SCANS_ERROR;
			return AM_errno;
		}
	}
	//close file
	OpenFileAr[fileDesc].filedesc = -1;
	free(OpenFileAr[fileDesc].filename);
	count_o--;

	CALL_OR_DIE(BF_CloseFile(fd));
	return AME_OK;
}


int AM_InsertEntry(int fileDesc, void *value1, void *value2) {

	if(fileDesc >= MAXOPENFILES || fileDesc < 0){
		printf("INDEX = %d\n", fileDesc);
		AM_errno = AME_WRONG_INDEX;
		return AM_errno;
	}

	//get file descriptor of wanted file.
	int fd;
	fd = OpenFileAr[fileDesc].filedesc;

	//check if there exists block0.
	int block_num;
	CALL_OR_DIE(BF_GetBlockCounter(fd, &block_num));
	if(block_num == 0){
		AM_errno = AME_INSERT_ERROR;
		return AM_errno;
	}

	if(block_num == 1){
		//in case we are have only the block0 in our file.
        	int root, ptr, ptr2;              		//block numbers of new data,index blocks.
		int err;
        	//create root
        	root = CreateBlock(fd, "index");
			printf("-Created index block, %d\n", root);
        	UpdateRoot(fd, root);
        	//create data block
        	ptr = CreateBlock(fd, "data");				//create rigth block, insert.
			printf("-Created data block, %d\n", ptr);
        	if(InsertData(ptr, value1, value2, OpenFileAr[fileDesc]) < 0){
            		AM_errno = AME_WRONG_INSERTION;
            		return AM_errno;
        	}
		ptr2 = CreateBlock(fd, "data");				//create left block / empty
		printf("-Created data block, %d\n", ptr2);

		char *data;
		BF_Block *block;
		BF_Block_Init(&block);
		CALL_OR_DIE(BF_GetBlock(fd, ptr, block));
		data = BF_Block_GetData(block);
		if((err = Cast_Insert(root, &data[9], &ptr, OpenFileAr[fileDesc], "index")) < 0){
			AM_errno = AME_BF_ERR;
      			return AM_errno;
		}							//Insert in root key, pointer.

		BF_Block *block1;
		BF_Block_Init(&block1);
		CALL_OR_DIE(BF_GetBlock(fd, root, block1));
		char *d_root;
		d_root = BF_Block_GetData(block1);
		d_root[5] = ptr2;					//Fix left pointer of root
		CALL_OR_DIE(BF_GetBlock(fd, ptr2, block1));
		data = BF_Block_GetData(block1);
		data[5] = ptr;

		CALL_OR_DIE(BF_UnpinBlock(block));
		CALL_OR_DIE(BF_UnpinBlock(block1));
		BF_Block_SetDirty(block1);
		BF_Block_SetDirty(block);

    	}
	else if(block_num > 1){
		int root, return_val;
		root = GetRoot(fd);
		BF_Block *block;
		BF_Block_Init(&block);
		CALL_OR_DIE(BF_GetBlock(fd, root, block));
		return_val = RecInsert(root, value1, value2, OpenFileAr[fileDesc]);
		if(return_val < 0){
			AM_errno = AME_WRONG_INSERTION;
            		return AM_errno;
		}
		else if(return_val > 0){
			int new_root, old_root;
			char *left = FindLeftmost(OpenFileAr[fileDesc], return_val);

        		//create root
        		new_root = CreateBlock(fd, "index");
			//old_root = GetRoot(fd);
			BF_Block *block1;
			BF_Block_Init(&block1);
			CALL_OR_DIE(BF_GetBlock(fd, new_root, block1));
			char *data;
			data = BF_Block_GetData(block1);

			data[5] = root;
			int err;
			if((err = Cast_Insert(new_root, left, &return_val, OpenFileAr[fileDesc], "index")) < 0){
                    		printf("\nError in Cast_Insert in BF level\n");
                    		return -1;
                	}
			UpdateRoot(fd, new_root);
			BF_Block_SetDirty(block1);
			BF_Block_SetDirty(block);
			CALL_OR_DIE(BF_UnpinBlock(block1));
			//old_root aristera, return_val deksia, root
			//cast insert sto root -----> letfmost, return_val
			//UpdateRoot

		}
		CALL_OR_DIE(BF_UnpinBlock(block));
	}

	return AME_OK;
}


int AM_OpenIndexScan(int fileDesc, int op, void *value) {

	int fd, root, start_b, goal_b, i, count;

	if(count_s > MAXSCANS){
		AM_errno = AME_MAX_OPEN_SCANS;
		return AM_errno;
	}

	for(i=0; i<MAXSCANS; i++){
		if(OpenScanAr[i].filedesc == -1){
			s_index = i;
			break;						//fix s_index to point to th 1st open position of the array.
		}
	}


	fd = OpenFileAr[fileDesc].filedesc;
	root = GetRoot(fd);
	//fix filedesc
	OpenScanAr[s_index].filedesc = fd;

	//test searching
	int test;
	test = RecSearch(root, value, OpenFileAr[fileDesc], NULL);
	if(BlockSearch(test, value, OpenFileAr[fileDesc]) < 0){
		AM_errno = AME_WRONG_SEARCH;
		count_s++;
		return s_index;
	}

	OpenScanAr[s_index].end_v = malloc(40);
	OpenScanAr[s_index].value = malloc(40);

	//printf("Open Scan: fd=%d, index=%d, op=%d\n", fd, s_index, op);
	if(op==1){
		//EQUAL: start=60, val=70, end_v=70;
		start_b = RecSearch(root, value, OpenFileAr[fileDesc], NULL);
		OpenScanAr[s_index].blocknum = start_b;

		BF_Block *block;
		BF_Block_Init(&block);
		CALL_OR_DIE(BF_GetBlock(fd, start_b, block));
		char *data = BF_Block_GetData(block);
		int j=9;
		while(Compare(value, &data[j], OpenFileAr[fileDesc].v1_type) != 0){
			j += OpenFileAr[fileDesc].v1_size + OpenFileAr[fileDesc].v2_size;
		}
		OpenScanAr[s_index].recnum = j;
		//ignore the same values / we want the next different
		while(Compare(value, &data[j], OpenFileAr[fileDesc].v1_type) == 0){
			j += OpenFileAr[fileDesc].v1_size + OpenFileAr[fileDesc].v2_size;
		}
		memcpy(OpenScanAr[s_index].value, &data[j], OpenFileAr[fileDesc].v1_size);
		memcpy(OpenScanAr[s_index].end_v, &data[j], OpenFileAr[fileDesc].v1_size);

		CALL_OR_DIE(BF_UnpinBlock(block));
	}
	else if(op==2){
		//NOT EQUAL: start=leftmost, val=60, end_v=NULL
		start_b = RecSearch(root, NULL, OpenFileAr[fileDesc], "left");
		OpenScanAr[s_index].blocknum = start_b;
		OpenScanAr[s_index].recnum = 9;

		goal_b = RecSearch(root, value, OpenFileAr[fileDesc], NULL);
		BF_Block *block;
		BF_Block_Init(&block);
		CALL_OR_DIE(BF_GetBlock(fd, goal_b, block));
		char *data = BF_Block_GetData(block);
		int j=9;
		while(Compare(value, &data[j], OpenFileAr[fileDesc].v1_type) != 0){
			j += OpenFileAr[fileDesc].v1_size + OpenFileAr[fileDesc].v2_size;
		}
		memcpy(OpenScanAr[s_index].value, &data[j], OpenFileAr[fileDesc].v1_size);
		OpenScanAr[s_index].end_v = NULL;
		CALL_OR_DIE(BF_UnpinBlock(block));
	}
	else if(op==3){
		//LESS: start=leftmost, val=60, end_v=60
		start_b = RecSearch(root, NULL, OpenFileAr[fileDesc], "left");
		OpenScanAr[s_index].blocknum = start_b;
		OpenScanAr[s_index].recnum = 9;

		goal_b = RecSearch(root, value, OpenFileAr[fileDesc], NULL);
		BF_Block *block;
		BF_Block_Init(&block);
		CALL_OR_DIE(BF_GetBlock(fd, goal_b, block));
		char *data = BF_Block_GetData(block);
		int j=9;
		while(Compare(value, &data[j], OpenFileAr[fileDesc].v1_type) != 0){
			j += OpenFileAr[fileDesc].v1_size + OpenFileAr[fileDesc].v2_size;
		}
		memcpy(OpenScanAr[s_index].end_v, &data[j], OpenFileAr[fileDesc].v1_size);
		memcpy(OpenScanAr[s_index].value, &data[j], OpenFileAr[fileDesc].v1_size);

		CALL_OR_DIE(BF_UnpinBlock(block));
	}
	else if(op==4){
		//GREATER: start=70, val=60, end_v=NULL
		start_b = RecSearch(root, value, OpenFileAr[fileDesc], NULL);
       		OpenScanAr[s_index].blocknum = start_b;

        	BF_Block *block;
		BF_Block_Init(&block);
		CALL_OR_DIE(BF_GetBlock(fd, start_b, block));
		char *data = BF_Block_GetData(block);
		int j=9;
		while(Compare(value, &data[j], OpenFileAr[fileDesc].v1_type) != 0){
			j += OpenFileAr[fileDesc].v1_size + OpenFileAr[fileDesc].v2_size;
		}
		while(Compare(value, &data[j], OpenFileAr[fileDesc].v1_type) == 0){
			j += OpenFileAr[fileDesc].v1_size + OpenFileAr[fileDesc].v2_size;
		}
		OpenScanAr[s_index].recnum = j;
		OpenScanAr[s_index].end_v = NULL;
		OpenScanAr[s_index].value = NULL;

		CALL_OR_DIE(BF_UnpinBlock(block));
	}
	else if(op == 5){
        	//LESS OR EQUAL: start=leftmost, val=70, end_v=70
        	start_b = RecSearch(root, NULL, OpenFileAr[fileDesc], "left");
		OpenScanAr[s_index].blocknum = start_b;
		OpenScanAr[s_index].recnum = 9;

        	goal_b = RecSearch(root, value, OpenFileAr[fileDesc], NULL);
        	BF_Block *block;
		BF_Block_Init(&block);
		CALL_OR_DIE(BF_GetBlock(fd, goal_b, block));
		char *data = BF_Block_GetData(block);
		int j=9;
		while(Compare(value, &data[j], OpenFileAr[fileDesc].v1_type) != 0){
			j += OpenFileAr[fileDesc].v1_size + OpenFileAr[fileDesc].v2_size;
		}
		while(Compare(value, &data[j], OpenFileAr[fileDesc].v1_type) == 0){
			j += OpenFileAr[fileDesc].v1_size + OpenFileAr[fileDesc].v2_size;
		}
		memcpy(OpenScanAr[s_index].end_v, &data[j], OpenFileAr[fileDesc].v1_size);
		memcpy(OpenScanAr[s_index].value, &data[j], OpenFileAr[fileDesc].v1_size);

		CALL_OR_DIE(BF_UnpinBlock(block));
		float f1, f2;
		f1 = *(float *)OpenScanAr[s_index].value;
		f2 = *(float *)OpenScanAr[s_index].end_v;
		printf("\tblokc=%d, offset=%d, fd=%d, value=%f, end_v=%f\n", OpenScanAr[i].blocknum,OpenScanAr[i].recnum,fd,f1,f2);
	}
	else if(op==6){
        	//GREATER OR EQUAL: start=60, val=NULL, end_v=NULL
        	start_b = RecSearch(root, value, OpenFileAr[fileDesc], NULL);
		OpenScanAr[s_index].blocknum = start_b;

		BF_Block *block;
		BF_Block_Init(&block);
		CALL_OR_DIE(BF_GetBlock(fd, start_b, block));
		char *data = BF_Block_GetData(block);
		int j=9;
		while(Compare(value, &data[j], OpenFileAr[fileDesc].v1_type) != 0){
			j += OpenFileAr[fileDesc].v1_size + OpenFileAr[fileDesc].v2_size;
		}
		OpenScanAr[s_index].recnum = j;
		OpenScanAr[s_index].end_v = NULL;
		OpenScanAr[s_index].value = NULL;

		CALL_OR_DIE(BF_UnpinBlock(block));
	}

    	count_s++;

	return s_index;
}


void *AM_FindNextEntry(int scanDesc) {

	if(AM_errno == AME_WRONG_SEARCH){
		AM_errno = AME_EOF;
		return NULL;
	}
	if(OpenScanAr[scanDesc].blocknum == -1){
		return NULL;
	}
	int fd, curr_b, offset, index, i, j;
	int t_count, flag=0;
	void *return_val;

	fd = OpenScanAr[scanDesc].filedesc;
	curr_b = OpenScanAr[scanDesc].blocknum;
	offset = OpenScanAr[scanDesc].recnum;

	BF_Block *block;
	BF_Block_Init(&block);

	BF_ErrorCode err;
	if((err = BF_GetBlock(fd, curr_b, block)) != BF_OK ){
		printf("PROVLIMA\n");
	}
	char *data = BF_Block_GetData(block);

	for(i=0; i<MAXOPENFILES; i++){
		if(OpenFileAr[i].filedesc == fd){
			break;
        	}
    	}
    	index = i;
	return_val = malloc(40);

    	//fix case of going out of block
    	i=9;
    	for(t_count=0; t_count<data[1]; t_count++){
        	if(i == offset){
            		break;
        	}
        	i += OpenFileAr[index].v1_size + OpenFileAr[index].v2_size;
    	}
    	if(t_count == data[1]-1){
        	flag=1;
    	}

    	//voithitika
    	BF_Block *block1;
    	BF_Block_Init(&block1);

    	if(Compare(&data[offset], OpenScanAr[scanDesc].end_v, OpenFileAr[index].v1_type) != 0){
		if(OpenFileAr[index].v1_type == 'f'){
			float f;
			memcpy(&f, &data[offset], OpenFileAr[index].v1_size);
		}
        	if(Compare(&data[offset], OpenScanAr[scanDesc].value, OpenFileAr[index].v1_type) != 0){
            		offset += OpenFileAr[index].v1_size;
            		AM_errno = AME_EOF;
            		///fix offset
            		if(flag==0){
                		OpenScanAr[scanDesc].recnum += OpenFileAr[index].v1_size + OpenFileAr[index].v2_size;
            		}
            		else if(flag==1){
                		OpenScanAr[scanDesc].recnum = 9;
						OpenScanAr[scanDesc].blocknum = data[5];
            		}

			//return data[offset];
			memcpy(return_val, &data[offset], OpenFileAr[index].v2_size);
            		BF_UnpinBlock(block);
			return return_val;
        	}
        	else{
            		while(Compare(&data[offset], OpenScanAr[scanDesc].value, OpenFileAr[index].v1_type) == 0){        ///while same values
                		if(flag==0){
                    			OpenScanAr[scanDesc].recnum += OpenFileAr[index].v1_size + OpenFileAr[index].v2_size;
                    			offset = OpenScanAr[scanDesc].recnum;
                    			t_count++;
                    			if(t_count == data[1]- 1){
                        			flag=1;
                    			}
                		}
                		else{
                    			OpenScanAr[scanDesc].recnum = 9;
                    			OpenScanAr[scanDesc].blocknum = data[5];
                   			BF_GetBlock(fd, data[5], block1);
                    			data = BF_Block_GetData(block1);
                    			t_count=0;
                    			flag=0;
					BF_UnpinBlock(block1);
                		}
            		}	

            		offset += OpenFileAr[index].v1_size;
            		AM_errno = AME_EOF;
            		///fix offset
            		if(flag=0){
                		OpenScanAr[scanDesc].recnum += OpenFileAr[index].v1_size + OpenFileAr[index].v2_size;
            		}
            		else{
                		OpenScanAr[scanDesc].recnum = 9;
                		OpenScanAr[scanDesc].blocknum = data[5];
            		}

            		//return data[offset];
			memcpy(return_val, &data[offset], OpenFileAr[index].v2_size);
			BF_UnpinBlock(block);
			return return_val;
        	}
    	}
    	else{
        	BF_UnpinBlock(block);
        	return NULL;
    	}
}


int AM_CloseIndexScan(int scanDesc) {

	if(scanDesc >= MAXSCANS || scanDesc < 0){
		printf("INDEX = %d\n", scanDesc);
		AM_errno = AME_WRONG_INDEX;
		return AM_errno;
	}
	int fd, i;
	fd = OpenScanAr[scanDesc].filedesc;
	//printf("Close Scan: fd=%d, index=%d\n", fd, scanDesc);

	OpenScanAr[scanDesc].filedesc = -1;
	free(OpenScanAr[scanDesc].end_v);
	free(OpenScanAr[scanDesc].value);
	count_s--;
	
	return AME_OK;
}


void AM_PrintError(char *errString) {

	int err;
	fprintf(stderr, "%s", errString);

	switch(AM_errno){
		case -1:
			fprintf(stderr, "AME_EOF: End Of File\n");
			break;
		case -2:
			fprintf(stderr, "AME_WRONG_ATTR: Type and Size of attribute given dont match.\n");
			break;
		case -3:
			fprintf(stderr, "AME_BF_ERROR: Error in BF level occured.\n");
			break;
		case -4:
			fprintf(stderr, "AME_MAX_OPEN_FILES: Already MAXOPENFILES open.\n");
			break;
		case -5:
			fprintf(stderr, "AME_NOT_AMFILE: File not of type AM.\n");
			break;
		case -6:
			fprintf(stderr, "AME_WRONG_INDEX: Wrong index given for array of Open Files.\n");
			break;
		case -7:
			fprintf(stderr, "AME_OPEN_SCANS_ERROR: Trying to close file with open scans.\n");
			break;
		case -8:
			fprintf(stderr, "AME_OPEN_FILE: Trying to destroy an opened file.\n");
			break;
		case -9:
			fprintf(stderr, "AME_INSERT_ERROR: Trying to insert values to a non created file.\n");
			break;
		case -10:
			fprintf(stderr, "AME_WRONG_INSERTION: Error in inserting values/key in block.\n");
			break;
        	case -11:
            		fprintf(stderr, "AME_MAX_OPEN_SCANS: Already MAXSCANS open\n");
            		break;
	}
}

void AM_Close() {

	/*int i;
	for(i=0; i<20; i++){
		free(OpenScanAr[i].value);
		free(OpenScanAr[i].end_v);
	}*/

	BF_Close();

	//free(OpenFileAr);
	//free(OpenScanAr);		//Destroy arrays.
}
