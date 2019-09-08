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
      AM_errno = AME_BF_ERR; \
      return AM_errno;       \
    }                         \
  }

#define MAXOPENFILES 20
#define MAXSCANS 20

//File with function Implementations that are used in our programm.

int Match(char type, int length){
	//Function to check if attributes are ok("i", "f" in [1,4] // "c" in [1,255])
	if(type=='i' || type=='f'){
		if(length<1 || length>4){
			return 0;
		}
	}
	else if(type=='c'){
		if(length<1 || length >255){
			return 0;
		}
	}
	return 1;
}

int Compare(void *v1, void *v2, char type)
{
	//last addition, if one of the two values is NULL, its is "greater" than the other.
	if(v2==NULL)
		return 2;
	else if(v1==NULL)
		return 1;

	//function that compares two void* values and returns 1, 2 or 0 accordingly.
	if(type == 'i'){
		int x1 = *(int *)v1;
		int x2 = *(int *)v2;
		if(x1 > x2)
			return 1;
		else if(x1 < x2)
			return 2;
		else
			return 0;
	}
	else if(type == 'f'){
		float x1 = *(float *)v1;
		float x2 = *(float *)v2;
		if(x1 > x2)
			return 1;
		else if(x1 < x2)
			return 2;
		else
			return 0;
	}
	else if(type == 'c'){
		char *x1 = (char *)v1;
		char *x2 = (char *)v2;

		if(strcmp(x1, x2) > 0)
           		 return 1;
        	else if(strcmp(x1, x2) < 0)
            		return 2;
		else if(strcmp(x1, x2) == 0)
			return 0;
	}
}

int CreateBlock(int fd, char *input)
{
	//Functions that creates a block specified by input -> "data" for data block, "index" for index block.
	BF_Block *block;
	BF_Block_Init(&block);
	char *data;
	int block_n;

	CALL_OR_DIE(BF_AllocateBlock(fd, block));
	data = BF_Block_GetData(block);
	if(strcmp(input, "index") == 0){
		char c = 'i';
        	memcpy(&data[0], &c, sizeof(c));
	}
	else if(strcmp(input, "data") == 0){
		char c = 'd';
        	memcpy(&data[0], &c, sizeof(c));
	}
	else{
		fprintf(stderr, "Wrong input message given to CreateBlock.\n");
		return -1;
	}
    	data[1] = 0;                       ///counter /// memcpy(&data[1], &count, sizeof(count));
	data[5] = -1;
    	BF_Block_SetDirty(block);
	CALL_OR_DIE(BF_UnpinBlock(block));
	CALL_OR_DIE(BF_GetBlockCounter(fd, &block_n));
	BF_Block_Destroy(&block);

	return (block_n-1);
}

int UpdateRoot(int fd, int pos)
{
	//function to update root position in block0.
	BF_Block *block;
	BF_Block_Init(&block);
	char *data; int j=0;

	CALL_OR_DIE(BF_GetBlock(fd, 0, block));
	data = BF_Block_GetData(block);
    	// root position in block0 is in data[15].
    	data[15] = pos;
    	BF_Block_SetDirty(block);
    	CALL_OR_DIE(BF_UnpinBlock(block));
    	BF_Block_Destroy(&block);
}

int GetRoot(int fd)
{
	//Function that returns the position of Root(the block number).
	BF_Block *block;
	BF_Block_Init(&block);
	char *data; int pos, j;

	CALL_OR_DIE(BF_GetBlock(fd, 0, block));
	data = BF_Block_GetData(block);
	pos = data[15];

	CALL_OR_DIE(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);
	return pos;
}

int isFull(FileData x, int counter, char id)
{
	//this function checks if there is space available for insertion in either datablock, indexblock.
	if(id == 'd'){				//about data block.
		if(((x.v1_size + x.v2_size)*(counter+1) + 9) > BF_BLOCK_SIZE){
			return 1;		//full
		}
		return 0;
	}
	else if(id == 'i'){			//about index block.
		if(((x.v1_size + 4)*(counter+1) + 9) > BF_BLOCK_SIZE){
			return 1;		//full
		}
		return 0;
	}
}

int Cast_Insert(int block_num, void *v1, void *v2, FileData x, char *type)
{
	//Casts the value and inserts it in either data block, either index block(specified in type).
	//We assume there is space for insertion, it is checked outside the function.
	BF_Block *block;
	BF_Block_Init(&block);
	char *data;
	int count=0, j=9;

	CALL_OR_DIE(BF_GetBlock(x.filedesc, block_num, block));
	data = BF_Block_GetData(block);

    if(strcmp(type, "data") == 0){
        ///if block is a data block
        //casting value1
        if(x.v1_type == 'i'){
            //key is integer
        	int value1 = *(int *)v1;
            while(value1 >= data[j] && count<data[1]){
                j += x.v1_size + x.v2_size;
                count++;
            }

            if(count == data[1]){					//it means we are in the end of the block.
                memcpy(&data[j], &value1, x.v1_size);
                j += x.v1_size;				//keep j to insert value2.
            }
            else if(count < data[1]){
                char *d; int i, c=0;
                int k=j;
                d = malloc((x.v1_size + x.v2_size)*(data[1]-count));
                for(i=0; c<(data[1]-count); c++){
                    memcpy(&d[i], &data[k], x.v1_size);
                    i += x.v1_size;
                    k += x.v1_size;
                    memcpy(&d[i], &data[k], x.v2_size);
                    i += x.v2_size;
                    k += x.v2_size;
                }
                memcpy(&data[j], &value1, x.v1_size);
                j += x.v1_size;
                k = j + x.v2_size;
                c = 0;
                for(i=0; c<(data[1]-count); c++){
                    memcpy(&data[k], &d[i], x.v1_size);
                    i += x.v1_size;
                    k += x.v1_size;
                    memcpy(&data[k], &d[i], x.v2_size);
                    i += x.v2_size;
                    k += x.v2_size;
                }
                free(d);
            }
        }
        else if(x.v1_type == 'f'){
            //key is float
            float value1 = *(float *)v1;
	    float key;
	    memcpy(&key, &data[j], sizeof(float));
            while(value1 >= key && count<data[1]){
                j += x.v1_size + x.v2_size;
                count++;
		memcpy(&key, &data[j], sizeof(float));
            }
            if(count == data[1]){					//it means we are in the end of the block.
                memcpy(&data[j], &value1, x.v1_size);
                j += x.v1_size;						//keep j to insert value2.
            }
            else if(count < data[1]){
                char *d; int i, c=0;
                int k=j;
                d = malloc((x.v1_size + x.v2_size)*(data[1]-count));
                for(i=0; c<(data[1]-count); c++){
                    memcpy(&d[i], &data[k], x.v1_size);
                    i += x.v1_size;
                    k += x.v1_size;
                    memcpy(&d[i], &data[k], x.v2_size);
                    i += x.v2_size;
                    k += x.v2_size;
                }
                memcpy(&data[j], &value1, x.v1_size);
                j += x.v1_size;
                k = j + x.v2_size;
                c = 0;
                for(i=0; c<(data[1]-count); c++){
                    memcpy(&data[k], &d[i], x.v1_size);
                    i += x.v1_size;
                    k += x.v1_size;
                    memcpy(&data[k], &d[i], x.v2_size);
                    i += x.v2_size;
                    k += x.v2_size;
                }
                free(d);
            }
        }
        else if(x.v1_type == 'c'){
            //key is string
            char *value1 = (char *)v1;
            while(strcmp(value1, &data[j])>=0 && count<data[1]){
                j += x.v1_size + x.v2_size;
                count++;
            }
            if(count == data[1]){					//it means we are in the end of the block.
                memcpy(&data[j], value1, x.v1_size);
                j += x.v1_size;						//keep j to insert value2.
            }
            else if(count < data[1]){
                char *d; int i, c=0;
                int k=j;
                d = malloc((x.v1_size + x.v2_size)*(data[1]-count));
                for(i=0; c<(data[1]-count); c++){
                    memcpy(&d[i], &data[k], x.v1_size);
                    i += x.v1_size;
                    k += x.v1_size;
                    memcpy(&d[i], &data[k], x.v2_size);
                    i += x.v2_size;
                    k += x.v2_size;
                }
                memcpy(&data[j], value1, x.v1_size);
                j += x.v1_size;
                k = j + x.v2_size;
                c = 0;
                for(i=0; c<(data[1]-count); c++){
                    memcpy(&data[k], &d[i], x.v1_size);
                    i += x.v1_size;
                    k += x.v1_size;
                    memcpy(&data[k], &d[i], x.v2_size);
                    i += x.v2_size;
                    k += x.v2_size;
                }
                free(d);
            }
        }
        //casting value2
        if(x.v2_type == 'i'){
            int value2 = *(int *)v2;
            memcpy(&data[j], &value2, x.v2_size);		//write value2
        }
        else if(x.v2_type == 'f'){
            float value2 = *(float *)v2;
            memcpy(&data[j], &value2, x.v2_size);
        }
        else if(x.v2_type == 'c'){
            char *value2 = (char *)v2;
            memcpy(&data[j], value2, x.v2_size);
        }
        data[1] += 1;
	BF_Block_SetDirty(block);
        CALL_OR_DIE(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);
        return 0;
    }
    else if(strcmp(type, "index")==0){
        ///if block in an index block.
        //casting value1
        if(x.v1_type == 'i'){
            //key is integer
        	int value1 = *(int *)v1;
            while(value1 >= data[j] && count<data[1]){
                j += x.v1_size + 4;
                count++;
            }

            if(count == data[1]){					//it means we are in the end of the block.
                memcpy(&data[j], &value1, x.v1_size);
                j += x.v1_size;						//keep j to insert value2.
            }
            else if(count < data[1]){
                char *d; int i, c=0;
                int k=j;
                d = malloc((x.v1_size + 4)*(data[1]-count));
                for(i=0; c<(data[1]-count); c++){
                    memcpy(&d[i], &data[k], x.v1_size);
                    i += x.v1_size;
                    k += x.v1_size;
                    memcpy(&d[i], &data[k], sizeof(int));
                    i += 4;
                    k += 4;
                }
                memcpy(&data[j], &value1, x.v1_size);
                j += x.v1_size;
                k = j + 4;
                c = 0;
                for(i=0; c<(data[1]-count); c++){
                    memcpy(&data[k], &d[i], x.v1_size);
                    i += x.v1_size;
                    k += x.v1_size;
                    memcpy(&data[k], &d[i], sizeof(int));
                    i += 4;
                    k += 4;
                }
                free(d);
            }
        }
        else if(x.v1_type == 'f'){
            //key is float
            float value1 = *(float *)v1;
	    float key;
	    memcpy(&key, &data[j], sizeof(float));
            while(value1 >= key && count<data[1]){
                j += x.v1_size + 4;
                count++;
		memcpy(&key, &data[j], sizeof(float));
            }
            if(count == data[1]){					//it means we are in the end of the block.
                memcpy(&data[j], &value1, x.v1_size);
                j += x.v1_size;						//keep j to insert value2.
            }
            else if(count < data[1]){
                char *d; int i, c=0;
                int k=j;
                d = malloc((x.v1_size + 4)*(data[1]-count));
                for(i=0; c<(data[1]-count); c++){
                    memcpy(&d[i], &data[k], x.v1_size);
                    i += x.v1_size;
                    k += x.v1_size;
                    memcpy(&d[i], &data[k], sizeof(int));
                    i += 4;
                    k += 4;
                }
                memcpy(&data[j], &value1, x.v1_size);
                j += x.v1_size;
                k = j + 4;
                c = 0;
                for(i=0; c<(data[1]-count); c++){
                    memcpy(&data[k], &d[i], x.v1_size);
                    i += x.v1_size;
                    k += x.v1_size;
                    memcpy(&data[k], &d[i], sizeof(int));
                    i += 4;
                    k += 4;
                }
                free(d);
            }
        }
        else if(x.v1_type == 'c'){
            //key is string
            char *value1 = (char *)v1;
            while(strcmp(value1, &data[j])>=0 && count<data[1]){
                j += x.v1_size + 4;
                count++;
            }
            if(count == data[1]){					//it means we are in the end of the block.
                memcpy(&data[j], value1, x.v1_size);
                j += x.v1_size;						//keep j to insert value2.
            }
            else if(count < data[1]){
                char *d; int i, c=0;
                int k=j;
                d = malloc((x.v1_size + 4)*(data[1]-count));
                for(i=0; c<(data[1]-count); c++){
                    memcpy(&d[i], &data[k], x.v1_size);
                    i += x.v1_size;
                    k += x.v1_size;
                    memcpy(&d[i], &data[k], sizeof(int));
                    i += 4;
                    k += 4;
                }
                memcpy(&data[j], value1, x.v1_size);
                j += x.v1_size;
                k = j + 4;
                c = 0;
                for(i=0; c<(data[1]-count); c++){
                    memcpy(&data[k], &d[i], x.v1_size);
                    i += x.v1_size;
                    k += x.v1_size;
                    memcpy(&data[k], &d[i], sizeof(int));
                    i += 4;
                    k += 4;
                }
                free(d);
            }
        }
        //casting value2
        int ptr = *(int *)v2;
        memcpy(&data[j], &ptr, sizeof(ptr));
        data[1] += 1;
        BF_Block_SetDirty(block);
        CALL_OR_DIE(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);
        return 0;
    }
}

int InsertData(int block_num, void *value1, void *value2, FileData x)
{
    	//Insert values to a data block.
	//Returns -1 if error,0 if value was inserted without errors or breaks, and number>0 if the data block split->returns the new block_num
    	BF_Block *block;
	BF_Block_Init(&block);
	char *data; int j=9;

	CALL_OR_DIE(BF_GetBlock(x.filedesc, block_num, block));
	data = BF_Block_GetData(block);

	if(data[0] != 'd'){
        	return -1;                                  ///if not data block?
	}


	if(isFull(x, data[1], 'd') == 0){
		int err;
		if((err = Cast_Insert(block_num, value1, value2, x, "data")) < 0){
			printf("\nError in Cast_Insert in BF level\n");
			return -1;
		}
	}
	else{
		int i, new_count, c=0, pos, l, r, lc=0, rc=0;
		int err;

		for(i=0; i<data[1]/2; i++){
			j += x.v1_size + x.v2_size;
		}
		l=j; r=j;
		//fix pos
		if(Compare(value1, &data[j], x.v1_type) == 1){
			pos=2;
		}
		else{
			pos=1;
		}
		if((data[1]+1)%2 == 0){
			while(Compare(&data[l], &data[l-(x.v1_size+x.v2_size)], x.v1_type) == 0 && lc <= data[1]/2){
				lc++;
				l -= x.v1_size+x.v2_size;
			}
			while(Compare(&data[r], &data[r+(x.v1_size+x.v2_size)], x.v1_type) == 0 && rc < data[1]/2){
				rc++;
				r += x.v1_size+x.v2_size;
			}
			if(lc == 0 && rc == 0){
				//classic
				if(pos == 2){
            				new_count = data[1]/2 + 1;
					j += x.v1_size + x.v2_size;
				}
				else{
					new_count = data[1]/2;
				}
			}
        		else if(lc !=0 || rc != 0){
				//not classic
				if(lc > rc){
					j = r + x.v1_size + x.v2_size;
					new_count = data[1]/2 + rc +1;
					if(Compare(value1, &data[j], x.v1_type) == 0){
						pos = 1;
					}
				}
				else{
					j = l;
					new_count = data[1]/2 - lc;
					if(Compare(value1, &data[j], x.v1_type) == 0){
						pos = 2;
					}
				}
        		}
		}
		else{
			while(Compare(&data[l], &data[l-(x.v1_size+x.v2_size)], x.v1_type) == 0 && lc <= data[1]/2){
				lc++;
				l -= x.v1_size+x.v2_size;
			}
			while(Compare(&data[r], &data[r+(x.v1_size+x.v2_size)], x.v1_type) == 0 && rc < data[1]/2){
				rc++;
				r += x.v1_size+x.v2_size;
			}
			if(lc == 0 && rc == 0){
				//classic
				if(pos == 2){
            				new_count = data[1]/2;
				}
				else{
					new_count = data[1]/2 -1;
					j -= x.v1_size + x.v2_size;
				}
			}
			else if(lc !=0 || rc != 0){
				//not classic
				if(lc > rc){
					j = r + x.v1_size + x.v2_size;
					new_count = data[1]/2 + rc +1;
					if(Compare(value1, &data[j], x.v1_type) == 0){
						pos = 1;
					}
				}
				else{
					j = l;
					new_count = data[1]/2 - lc;
					if(Compare(value1, &data[j], x.v1_type) == 0){
						pos = 2;
					}
				}
			}
		}

		int new_b;
		new_b = CreateBlock(x.filedesc, "data");
		BF_Block *block1;
		BF_Block_Init(&block1);
		CALL_OR_DIE(BF_GetBlock(x.filedesc, new_b, block1));

		char *d;
		d = BF_Block_GetData(block1);
		for(i=9; c<=(data[1]-new_count); c++){
			memcpy(&d[i], &data[j], x.v1_size);		    //copy 1st value.
			i += x.v1_size;
			j += x.v1_size;
			memcpy(&d[i], &data[j], x.v2_size);		    //copy 2nd value.
			i += x.v2_size;
			j += x.v2_size;
		}
		d[1] = c-1;
		d[5] = data[5];
		data[1] = new_count;				//counters
		data[5] = new_b;				//fix data block pointer.

		if(pos == 2){	                    		//if value1 > 1st key of new block, values goes in it.
			if((err = Cast_Insert(new_b, value1, value2, x, "data")) < 0){
				printf("\nError in Cast_Insert in BF level\n");
				return -1;
			}
		}
		else if(pos == 1){				//value1 <1st key of new block, it goes in the first block that we already had.
			if((err = Cast_Insert(block_num, value1, value2, x, "data")) < 0){
				printf("\nError in Cast_Insert in BF level\n");
				return -1;
			}
		}

		BF_Block_SetDirty(block1);
		BF_Block_SetDirty(block);
    		CALL_OR_DIE(BF_UnpinBlock(block1));
		CALL_OR_DIE(BF_UnpinBlock(block));
		BF_Block_Destroy(&block);
		BF_Block_Destroy(&block1);
		return new_b;					//return the block number of new block in order to fix pointers.
	}
	CALL_OR_DIE(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);

        return 0;
}

char *FindLeftmost(FileData x, int block_num)
{
   	///returns Leftmost block_number, traversing the tree statring from the block with block_num.
    	BF_Block *block;
	BF_Block_Init(&block);
	char *data;
	BF_GetBlock(x.filedesc, block_num, block);
	data = BF_Block_GetData(block);

	char *d;
	d = malloc(x.v1_size);
	if(data[0] == 'd'){
		memcpy(d, &data[9], x.v1_size);
		return d;
	}
	else{
		d = FindLeftmost(x, data[5]);
	}
	BF_UnpinBlock(block);
	BF_Block_Destroy(&block);
	return d;
}

int RecInsert(int block_num, void *value1, void *value2, FileData x)
{
	//Recursive function that handles the general case of block insertion.
	//Starts from block with block_num, recursively goes down the tree and handles all the cases of index block insertions.
	//Returns to the function InsertEntry value<0 if error, 0 if everything was okay, or positive number in case of root block split, 
	//which is handled there.
	char *data;
	int return_val;
	BF_Block *block;
	BF_Block_Init(&block);
	CALL_OR_DIE(BF_GetBlock(x.filedesc, block_num, block));
	data = BF_Block_GetData(block);
	if(data[0] == 'd'){
		//we are in data block
		BF_Block_Destroy(&block);
		return InsertData(block_num, value1, value2, x);
	}
	else if(data[0] == 'i'){
		//we are in index block
		int count=0, j=9;
		while(count<data[1] && Compare(value1,&data[j],x.v1_type)==1){		//fix CastInsert???
			j += x.v1_size + 4;
			count++;
		}
		return_val = RecInsert(data[j-4], value1, value2, x);
	}
	if(return_val < 0){
        	//error
		return -1;
	}
	else if(return_val == 0){
		BF_Block_Destroy(&block);
		return 0;
	}
	else{
        	if(isFull(x, data[1], 'i') == 0){

			int err;
			char *left_val = FindLeftmost(x, return_val);


			if((err = Cast_Insert(block_num, left_val, &return_val, x, "index")) < 0){
                    		printf("\nError in Cast_Insert in BF level\n");
                    		return -1;
                	}
			BF_Block_SetDirty(block);
			BF_Block_Destroy(&block);
			return 0;
        	}
        	else{
			//index block is full / split
			char *left_val = FindLeftmost(x, return_val);
			int j, i, new_count, c=0;

			//split --> create new: new_b, data_new, block1
			int new_b;
			new_b = CreateBlock(x.filedesc, "index");
			char *data_new;
			BF_Block *block1;
			BF_Block_Init(&block1);
			CALL_OR_DIE(BF_GetBlock(x.filedesc, new_b, block1));
			data_new = BF_Block_GetData(block1);

			//fix offset
			if((data[1]%2) != 0){
				j = 9 + (data[1]/2)*(x.v1_size+4);
			}
			else{
				j = 9 + (data[1]/2 - 1)*(x.v1_size+4);
			}
			if(Compare(left_val, &data[j], x.v1_type) == 2){
				j += x.v1_size;
				data_new[5] = data[j];
				j += 4;
				for(i=9; c<=data[1]/2; c++){
					//copy to new block
					memcpy(&data_new[i], &data[j], x.v1_size);
					i += x.v1_size;
					j += x.v1_size;
					memcpy(&data_new[i], &data[j], sizeof(int));
					i += 4;
					j += 4;
				}
				data_new[1] = c-1;				//???
				data[1] = data[1] - data_new[1] -1;
				int err;
				if((err = Cast_Insert(block_num, left_val, &return_val, x, "index")) < 0){
                    			printf("\nError in Cast_Insert in BF level\n");
                    			return -1;
                		}
				BF_Block_SetDirty(block1);
				BF_Block_SetDirty(block);
			}
			else{
				j += x.v1_size + 4;
				if(Compare(left_val, &data[j], x.v1_type) == 2){
					data_new[5] = return_val;
					for(i=9; c<=data[1]/2; c++){
						//copy to new block
						memcpy(&data_new[i], &data[j], x.v1_size);
						i += x.v1_size;
						j += x.v1_size;
						memcpy(&data_new[i], &data[j], sizeof(int));
						i += 4;
						j += 4;
					}
					data_new[1] = c-1;
					data[1] = data[1] - (data_new[1]);
					BF_Block_SetDirty(block1);
					BF_Block_SetDirty(block);
				}
				else{
					j += x.v1_size;
					data_new[5] = data[j];
					j += 4;
					for(i=9; c<data[1]/2; c++){
						//copy to new block
						memcpy(&data_new[i], &data[j], x.v1_size);
						i += x.v1_size;
						j += x.v1_size;
						memcpy(&data_new[i], &data[j], sizeof(int));
						i += 4;
						j += 4;
					}
					data_new[1] = c-1;
					data[1] = data[1] - data_new[1] - 1;

					if(new_b==14){
						int l, errikos=0;
						for(l=9; errikos<data_new[1]; errikos++){
							printf("%s,  ", &data_new[l]);
							l += x.v1_size+4;
						}
						printf("\n");
	   				}

					int err;
					if((err = Cast_Insert(new_b, left_val, &return_val, x, "index")) < 0){
                    				printf("\nError in Cast_Insert in BF level\n");
                    				return -1;
                			}
					BF_Block_SetDirty(block1);
					BF_Block_SetDirty(block);
				}
			}
			CALL_OR_DIE(BF_UnpinBlock(block));
			CALL_OR_DIE(BF_UnpinBlock(block1));
			BF_Block_Destroy(&block);
			BF_Block_Destroy(&block1);
			return new_b;
        	}
	}
}

void PrintAll1(int index)
{
	//Test functions that prints all block(and their values) with the order they have in memory.
	int fd = OpenFileAr[index].filedesc;
	int i, block_num;

	BF_GetBlockCounter(fd, &block_num);
	for(i=1; i<block_num; i++){
		BF_Block *block;
		BF_Block_Init(&block);
		BF_GetBlock(fd, i, block);
		char *data = BF_Block_GetData(block);
		printf("Hello, block number=%d, id=%c, counter=%d\n", i, data[0], data[1]);
		int root = GetRoot(fd);
		if(i == root){
			printf("I am the root!!(block number=%d)--- 1st ptr=%d\n", i, data[5]);
		}
		int l, offset = 9;
		for(l=0; l<data[1]; l++) {
			printf("(key, value) = (");
			if(OpenFileAr[index].v1_type == 'i'){
				printf("%d", data[offset]);
			}
			else if(OpenFileAr[index].v1_type == 'f'){
				float f;
				memcpy(&f, &data[offset], sizeof(float));
				printf("%f", f);
			}
			else if(OpenFileAr[index].v1_type == 'c'){
				char *e;
				e = malloc(OpenFileAr[index].v1_size);
				memcpy(e, &data[offset], OpenFileAr[index].v1_size);
				printf("%s", e);
				free(e);
			}
			printf(",");

			offset += OpenFileAr[index].v1_size;
			if(data[0] == 'i'){
				printf("%d", data[offset]);
			}
			else{
				if(OpenFileAr[index].v2_type == 'i'){
					printf("%d", data[offset]);
				}
				else if(OpenFileAr[index].v2_type == 'f'){
					float f;
					memcpy(&f, &data[offset], sizeof(float));
					printf("%f", f);
				}
				else if(OpenFileAr[index].v2_type == 'c'){
					char *e;
					e = malloc(OpenFileAr[index].v2_size);
					memcpy(e, &data[offset], OpenFileAr[index].v2_size);
					printf("%s", e);
					free(e);
				}
			}
			printf(")\n");
			if(data[0] == 'd')
				offset += OpenFileAr[index].v2_size;
			else
				offset += 4;

			BF_UnpinBlock(block);
		}
	}
	//BF_Block_Destroy(&block);
}

void Print(int cur_b_num, int index) {
    //Function-component of PrintAll
    int fd = OpenFileAr[index].filedesc;
    BF_Block *block;
    BF_Block_Init(&block);
    BF_GetBlock(fd, cur_b_num, block);
    char *data = BF_Block_GetData(block);
    if(data[0] == 'i') {
        printf("Printing index block %d...\n", cur_b_num);
    }else if(data[0] == 'd') {
        printf("Printing data block %d...\n", cur_b_num);
    }else {
        fprintf(stderr, "BLOCK ERROR! Wrong id(%c).", data[0]);
        exit(1);
    }
    int offset = 9;
    printf("|%d|", data[5]);
    for(int i=0; i<data[1]; i++) {
        //Printing value1
        if(OpenFileAr[index].v1_type == 'i') {
            printf("%d|", data[offset]);
        }else if(OpenFileAr[index].v1_type == 'f') {
            float f;
	    memcpy(&f, &data[offset], sizeof(float));
	    printf("%.1f|", f);
        }else if(OpenFileAr[index].v1_type == 'c') {
            char *e;
    		e = malloc(OpenFileAr[index].v1_size);
			memcpy(e, &data[offset], OpenFileAr[index].v1_size);
			printf("%s|", e);
			free(e);
        }
        //Printing value2
        offset += OpenFileAr[index].v1_size;

	if(data[0] == 'i'){
		printf("%d|", data[offset]);
	}
	else{
        	if(OpenFileAr[index].v2_type == 'i'){
    			printf("%d|", data[offset]);
		}
		else if(OpenFileAr[index].v2_type == 'f'){
			float f;
			memcpy(&f, &data[offset], sizeof(float));
			printf("%.1f|", f);
		}else if(OpenFileAr[index].v2_type == 'c'){
			char *e;
			e = malloc(OpenFileAr[index].v2_size);
			memcpy(e, &data[offset], OpenFileAr[index].v2_size);
			printf("%s|", e);
			free(e);
		}
	}
        if(data[0] == 'd')
				offset += OpenFileAr[index].v2_size;
		else
				offset += 4;
	}
	BF_UnpinBlock(block);
	BF_Block_Destroy(&block);
	printf("\n");
}

void recPrint(int block_num, int index) {
    //Function-component of PrintAll
    BF_Block *block;
	BF_Block_Init(&block);
    int fd = OpenFileAr[index].filedesc;
    BF_GetBlock(fd, block_num, block);
    char *data = BF_Block_GetData(block);
    if(data[0] == 'i') {
        int offset = 5;
        for(int i=0; i<=data[1]; i++) {
            //Print the leaf
            Print(data[offset], index);
            recPrint(data[offset], index);
            offset += 4 + OpenFileAr[index].v1_size;
        }
    }
    BF_UnpinBlock(block);
    BF_Block_Destroy(&block);
}

void PrintAll(int index) {
    //(!) Function that prints the block in order of their physical representation.
    //The leftmost data bock first etc. so that the values will so up in the right order if the tree has been built correclty.

    //Getting the file desc
    int fd = OpenFileAr[index].filedesc;
    //Print the number of blocks
    int block_num;
    BF_GetBlockCounter(fd, &block_num);
    printf("This B+ Tree contains %d blocks(leafs)\n", block_num);

    //Call the recursive function that prints each leaf
    //Start from the root
    //Get the root number

    BF_Block *block;
	BF_Block_Init(&block);
    BF_GetBlock(fd, 0, block);
    char *data = BF_Block_GetData(block);
    printf("Starting recursive print from the root block with number %d\n", data[15]);
    //Print the root leaf
    Print(data[15], index);
    //Start recPrint for the other leafs
    recPrint(data[15], index);

    printf("Finished!\n");
    BF_UnpinBlock(block);
    BF_Block_Destroy(&block);
}

int BlockSearch(int block_num, void *v, FileData x)
{
	//Searches for a value in a data block.
	int j=9, fd, count;
	fd = x.filedesc;
	BF_Block *block;
	BF_Block_Init(&block);
	CALL_OR_DIE(BF_GetBlock(fd, block_num, block));
	char *data;
	data = BF_Block_GetData(block);

	for(count=0; count<data[1]; count++){
		if(Compare(v, &data[j], x.v1_type) == 0){
			CALL_OR_DIE(BF_UnpinBlock(block));
			return 1;
		}
		j += x.v1_size + x.v2_size;
	}
	CALL_OR_DIE(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);
	return -1;
}

int RecSearch(int block_num, void *v, FileData x, char *type)
{
	//Recursive function that searches for the block_number that should contain the value if it existed in tree.
	int j=9, fd, count=0, return_val;

	fd = x.filedesc;
	BF_Block *block;
	BF_Block_Init(&block);
	CALL_OR_DIE(BF_GetBlock(fd, block_num, block));
	char *data;
	data = BF_Block_GetData(block);

	if(type == NULL){
		if(data[0] == 'i'){
			while(Compare(v, &data[j], x.v1_type) != 2 && count<data[1]){
				count++;
				j += x.v1_size + 4;
			}
			return_val = RecSearch(data[j-4], v, x, NULL);
		}
		else if(data[0] == 'd'){
			BF_Block_Destroy(&block);
			return block_num;
		}
	}
	else{
		if(data[0] == 'i'){
			return_val = RecSearch(data[5], v, x, type);
		}
		else if(data[0] == 'd'){
			BF_Block_Destroy(&block);
			return block_num;
		}
	}
	CALL_OR_DIE(BF_UnpinBlock(block));
	BF_Block_Destroy(&block);
	return return_val;
}
