#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "AM.h"

int Match(char type, int length);
int Compare(void *v1, void *v2, char type);
int CreateBlock(int fd, char *input);
int UpdateRoot(int fd, int pos);
int GetRoot(int fd);
int isFull(struct FileData x, int counter, char id);
int Cast_Insert(int block_num, void *v1, void *v2, struct FileData x, char *type);
int InsertData(int block_num, void *value1, void *value2, struct FileData x);
char *FindLeftmost(struct FileData x, int block_num);
int RecInsert(int block_num, void *value1, void *value2, struct FileData x);
void PrintAll1(int index);
void Print(int cur_b_num, int index);
void recPrint(int block_num, int index);
void PrintAll(int index);
int BlockSearch(int block_num, void *v, struct FileData x);
int RecSearch(int block_num, void *v, struct FileData x, char *type);

#endif
