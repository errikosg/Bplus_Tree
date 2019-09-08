#ifndef AM_H_
#define AM_H_

/* Error codes */

extern int AM_errno;

#define AME_OK 0
#define AME_EOF -1
#define AME_WRONG_ATTR -2		//wrong attribute given in CreateIndex-type doesnt match size standards
#define AME_BF_ERR -3			//error occured in bf level
#define AME_MAX_OPEN_FILES -4		//already MAXOPENFILES open
#define AME_NOT_AMFILE -5		//file is not of type AM
#define AME_WRONG_INDEX -6		//wrong index given for OpenFiles array
#define AME_OPEN_SCANS_ERROR -7		//trying to close file while it has open scans
#define AME_OPEN_FILE -8		//trying to destroy an open file
#define AME_INSERT_ERROR -9		//trying to insert value to a non created file / without block0.
#define AME_WRONG_INSERTION -10		//error in values/key insertion in a block
#define AME_MAX_OPEN_SCANS -11		//alreasy MAXSCANS open
#define AME_WRONG_SEARCH -12		//wrong search / doesnt exist in file

#define EQUAL 1
#define NOT_EQUAL 2
#define LESS_THAN 3
#define GREATER_THAN 4
#define LESS_THAN_OR_EQUAL 5
#define GREATER_THAN_OR_EQUAL 6

#define MAXOPENFILES 20
#define MAXSCANS 20

typedef struct FileData{
	char v1_type, v2_type;
	int v1_size, v2_size;
	int filedesc;
	char *filename;
}FileData;				//metadata of open files.

FileData OpenFileAr[MAXOPENFILES];			//array of open files.

typedef struct ScanData{
	int filedesc, blocknum, recnum;
	void *value; void *end_v;
}ScanData;				//data of open scans.

ScanData OpenScanAr[MAXSCANS];			//array of open scans.

void AM_Init( void );


int AM_CreateIndex(
  char *fileName, /* όνομα αρχείου */
  char attrType1, /* τύπος πρώτου πεδίου: 'c' (συμβολοσειρά), 'i' (ακέραιος), 'f' (πραγματικός) */
  int attrLength1, /* μήκος πρώτου πεδίου: 4 γιά 'i' ή 'f', 1-255 γιά 'c' */
  char attrType2, /* τύπος πρώτου πεδίου: 'c' (συμβολοσειρά), 'i' (ακέραιος), 'f' (πραγματικός) */
  int attrLength2 /* μήκος δεύτερου πεδίου: 4 γιά 'i' ή 'f', 1-255 γιά 'c' */
);


int AM_DestroyIndex(
  char *fileName /* όνομα αρχείου */
);


int AM_OpenIndex (
  char *fileName /* όνομα αρχείου */
);


int AM_CloseIndex (
  int fileDesc /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
);


int AM_InsertEntry(
  int fileDesc, /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
  void *value1, /* τιμή του πεδίου-κλειδιού προς εισαγωγή */
  void *value2 /* τιμή του δεύτερου πεδίου της εγγραφής προς εισαγωγή */
);


int AM_OpenIndexScan(
  int fileDesc, /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
  int op, /* τελεστής σύγκρισης */
  void *value /* τιμή του πεδίου-κλειδιού προς σύγκριση */
);


void *AM_FindNextEntry(
  int scanDesc /* αριθμός που αντιστοιχεί στην ανοιχτή σάρωση */
);


int AM_CloseIndexScan(
  int scanDesc /* αριθμός που αντιστοιχεί στην ανοιχτή σάρωση */
);


void AM_PrintError(
  char *errString /* κείμενο για εκτύπωση */
);

void AM_Close();


#endif /* AM_H_ */
