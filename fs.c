#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "disk.h"

#define MAX_DIR 64
#define MAX_FILDES 32
#define MAX_F_NAME 15
#define BLOCK_SIZE 4096
#define EOFF -1
#define READ 0
#define WRITE 1

struct super_block
{
 int version; // this doesn't matter
 /* FAT information */
 int fat_idx;
 int fat_len;//4 becuse data blocks*
 /* directory information */
 int dir_idx;
 int dir_len;//Because int*char*int*int*int
 /* data information */
 int data_idx;
};

struct dir_entry //it stores the information of a file
{
 int used;
 char name[MAX_F_NAME + 1]; //file name
 int size;//file size
 int head; //first data block of this file
 int ref_cnt; //how many file descriptors this entry is using, if ref_cnt > 0 we cannot delete this file
};

struct file_descriptor
{
 int used; // file descriptor is in use
 int file; // the first block of current file who uses this file descriptor
 off_t offset;
}; 

//global variables
struct super_block fs;
struct file_descriptor fildes[MAX_FILDES]; //MAX_FILDES=32
int *FAT; // a queue
struct dir_entry *DIR; // a queue of directories
int files_open =0;
int mounted = 0;
char mountname[200];

//Functions
int make_fs(char *disk_name);
int mount_fs(char *disk_name);
int umount_fs(char *disk_name);
int fs_open(char *name);
int fs_close(int fd);
int fs_create(char *name);
int fs_delete(char *name);
int fs_read(int fd, void *buf, size_t nbyte);
int fs_write(int fd, void *buf, size_t nbyte);
int fs_get_filesize(int fd);
int fs_listfiles(char ***files);
int fs_lseek(int fd, off_t offset);
int fs_truncate(int fd, off_t length);
int dir_list_file(char * name);
int get_free_filedes();
void dir_create_file(char * name);
int get_free_FAT();
int fs_rdwr(int fd, void *dst, size_t nbyte,int mode);

int make_fs(char *disk_name)
{
	//error check
	if(make_disk(disk_name) == -1)
	{
      	fprintf(stderr,"make_fs: cannot make file\n");
		return -1;
	}
	if(open_disk(disk_name) == -1)
	{
		fprintf(stderr,"make_fs: cannot open file\n");
		return -1;
	}

	//////////setting up the superblock/////////
	fs.fat_idx = 2;
	fs.fat_len = 4;
	fs.dir_idx = 100;
	fs.dir_len = 1;
	fs.data_idx = 4096;

	/////////setting the super block to zero////////////////
	char * buf = (char*)malloc(BLOCK_SIZE);//allocate a buffer with block size
	memset(buf, 0, BLOCK_SIZE);//sets all the buffer to 0 
	memcpy(buf, &fs, sizeof(struct super_block));//copies the super block to buf
	block_write(0, buf);//writes the super block to the 0th table

	free(buf);//frees the buffer

	if(close_disk() == -1)//close the disk
	{
		fprintf(stderr,"make_fs: cannot close file\n");
		return -1;
	}

	return 0;
}

int mount_fs(char *disk_name)
{
	//error checking
	if(open_disk(disk_name) == -1)
	{
		fprintf(stderr,"mount_fs: cannot open file\n");
		return -1;
	}
	if(mounted == 1)
	{
		fprintf(stderr,"mount_fs: disk is already open\n");
		return -1;
	}

	///////load the super block//////////////
	char * buf = (char*)malloc(BLOCK_SIZE);//allocate a buffer with block size
	memset(buf, 0, BLOCK_SIZE);//set the buf to 0
	block_read(0, buf);//read from the disk to the buffer
	memcpy(&fs, buf, sizeof(struct super_block));//set the global 

	///////Loading FAT//////////
	//int size = fs.fat_len * sizeof(int);//get the length
	int blocks = 4;//(size - 1)/ BLOCK_SIZE + 1;//get how many blocks there are

	if ((FAT = (int *) malloc(blocks * BLOCK_SIZE)) == NULL)//check if there is a FAT
	{
 		perror("cannot allocate FAT\n");
 		exit(1);
 	}
 	//printf("fat address: %p\n", FAT);
	int cnt = 0;
	char *p = (char*) FAT;	
 	for ( ; cnt < blocks; ++cnt) 
 	{
 		memset(buf, 0, BLOCK_SIZE);//set buf to zero
 		block_read(fs.fat_idx + cnt, buf);//read the firts block to fuf
		memcpy(p, buf, BLOCK_SIZE);//put the buf to where FAT was
 		p += BLOCK_SIZE;//increament to next block
 	}

 	/////////////Load Directory/////////////
 	//size = fs.dir_len * sizeof(struct dir_entry);//get the length 
	blocks =  1;//get how many blocks there are

	if ((DIR = (struct dir_entry *) malloc(blocks * BLOCK_SIZE)) == NULL)//check if there is a DIR
	{
 		perror("cannot allocate DIR\n");
 		exit(1);
 	}

	cnt = 0;
	p = (char*) DIR;	
 	for ( ; cnt < blocks; ++cnt) 
 	{
 		memset(buf, 0, BLOCK_SIZE);//set buf to zero
 		block_read(fs.dir_idx + cnt, buf);//read the firts block to buf
		memcpy(p, buf, BLOCK_SIZE);//put the buf to where DIR was
 		p += BLOCK_SIZE;//increament to next block
 	}

 	//////////////Clearing File desctiprtore/////////////
 	cnt = 0;
 	for ( ; cnt < MAX_FILDES; ++cnt)
 	{
 		fildes[cnt].used = 0;
 	}

 	free(buf);
 	
 	close_disk();
 	/*if(close_disk()== -1)
 	{
 		fprintf(stderr,"mount_fs: cannot close file\n");
 		return -1;
 	}*/

	mounted = 1;
	strcpy(mountname, disk_name);
 	return 0;
}

int umount_fs(char *disk_name)
{
	if(strcmp(mountname, disk_name))
	{
		fprintf(stderr,"unmount_fs: name dont match\n");
		return -1;
	}
	if(mounted == 0)
	{
		fprintf(stderr,"unmount_fs: disk not mounted\n");
		return -1;
	}
	///////Closing the Files//////////
	int cnt = 0;
	for( ; cnt < MAX_FILDES; ++cnt)
	{
		if(fildes[cnt].used != 0)
		{
			fildes[cnt].used = 0;
		}
	}

	///////FAT WriteBack//////////////
	//int size = fs.fat_len * sizeof(int);//get the length
	int blocks = 4;//(size - 1)/ BLOCK_SIZE + 1;//get how many blocks there are
	
	char * buf = (char*)malloc(BLOCK_SIZE);//allocate a buffer with block size
	cnt = 0;
	char *p = (char*) FAT;	
 	for ( ; cnt < blocks; ++cnt) 
 	{
 		memset(buf, 0, BLOCK_SIZE);//sets all the buffer to 0 
		memcpy(buf, p, BLOCK_SIZE);//copies p to buf
		block_write(fs.fat_idx + cnt, buf);//writes the buf to the the fat place
		p += BLOCK_SIZE;
	}

	///////////DIR WriteBack////////////////////
	//size = MAX_DIR * sizeof(struct dir_entry);//get the length 
	blocks = 1;//(size - 1)/ BLOCK_SIZE + 1;//get how many blocks there are

	cnt = 0;
	char * d = (char*) DIR;	
 	for ( ; cnt < blocks; ++cnt) 
 	{
 		memset(buf, 0, BLOCK_SIZE);//set buf to zero
 		memcpy(buf, p, BLOCK_SIZE);//put the p to buf
 		block_write(fs.dir_idx + cnt, buf);//read the firts block to fuf
 		d += BLOCK_SIZE;//increament to next block
 	}

 	//printf("We here\n");
 	//printf("second fat address: %p\n", FAT);

 	/*
	for(cnt = 0; cnt < MAX_FILDES; ++cnt)
	{
		if(fildes[cnt].used != 0)
		{
			fildes[cnt].used = 0;
		}
	}
	*/

	free(buf);
 	//free(buf);
 	//printf("failed before\n");
 	free(FAT);
 	//printf("failed after\n");
 	free(DIR);
 	
 	close_disk();
 	/*
 	if(close_disk() != 0)
 	{
 		fprintf(stderr,"umount_fs: cannot close disk\n");
 		return -1;
 	}*/
 	mounted = 0;

 	return 0; 	
}

int dir_list_file(char * name)
{
	int i = 0;
	for( ; i < MAX_DIR; i++)
	{
		if(strcmp(name, DIR[i].name) == 0 && DIR[i].used == 1 )//if the name equal to any name in DIR 
			return i;
	}
	return -1;
}
int get_free_filedes()
{
	int i =0;
	for( ; i<MAX_FILDES; i++)
	{
		if(fildes[i].used == 0)
			return i;
	}
	return -1;
}

int fs_open(char *name)
{
	/////////Checking the FileName///////////
	int file = dir_list_file(name);//find the file

	if(file == -1)//error checking
	{
		fprintf(stderr,"fs_open: file name not found\n");
		return -1;
	}

	///////////Setting the file descripter/////////////////
	int fd = get_free_filedes();//finds a file desriptor

	if(fd == -1)//error checking
	{
		fprintf(stderr,"fs_open: cannot open more than 32 file descriptors\n");
		return -1;
	}

	fildes[fd].used = 1; //set it to used
  	fildes[fd].file = file;//set the file
  	fildes[fd].offset = 0;//no offset

	DIR[file].ref_cnt++;//inrease the refcount

	return fd;
}

int fs_close(int fd)
{
	if(fd < 0 || fd >= MAX_FILDES || fildes[fd].used == 0)//error checking
	{
		fprintf(stderr,"fs_close: some error in file descriptor\n");
    	return -1;
	}

	DIR[fildes[fd].file].ref_cnt--;//decrease the ref count of the file 
  	fildes[fd].used = 0;//set the fd to zero

  	return 0;
}

void dir_create_file(char * name)
{
	int i = 0;
	for(  ; i < MAX_DIR; i++)
	{
		if(DIR[i].used == 0)//when an empty dir place found
		{
			DIR[i].used = 1;//Mark it used
			strcpy(DIR[i].name, name);//copy name
			DIR[i].size = 0;//size equals to 0
			int fat_p = get_free_FAT();//find a free fat slot
			DIR[i].head = fat_p;//set the head to the free fat slot
			FAT[fat_p] = EOFF;//mark the slot as EOF
			DIR[i].ref_cnt = 0;//set the ref count to zero
			files_open++;//increase the dir_len
			return;
		}
	}
}

int get_free_FAT()
{
	int i;
 	for(i = 0; i < 4096; i++)
 	{
    	if(FAT[i] == 0)
      		return i;
    }

  fprintf(stderr,"get_free_FAT: Not enough place at FAT\n");

  return -1;
}

int fs_create(char *name)
{
	int num_char = strlen(name);
	if(num_char > MAX_F_NAME)//checking the legnth of the file 
	{
		fprintf(stderr,"fs_create: filename too long\n");
		return -1;
	}

	if(files_open >= MAX_DIR) //checks if enough spaces
	{
		fprintf(stderr,"dir_create_file: cannot create more than 64 files\n");
		return -1;
	}

	int file = dir_list_file(name);// find the file in DIR

	if(file < 0)//if the file doesnt exists
	{
		dir_create_file(name);//create file
		return 0;
	}
	else//means the file exists
	{
		fprintf(stderr,"fs_create: file already exists\n");
		return -1;
	}
}

int fs_delete(char *name)
{
	int file = dir_list_file(name);//find the file
	if(DIR[file].ref_cnt > 0)
    {
      	fprintf(stderr,"dir_delete_file: file [%s] is in use: ref_cnt [%i]\n",name,DIR[file].ref_cnt);
     	 return -1;
    }
	if(file < 0)
	{
		fprintf(stderr,"fs_delete: file [%s] does not exist\n",name);
    	return -1;
	}
	else
	{
		DIR[file].used = 0;//change to not used
		DIR[file].size = 0;//change size to 0
		int current = DIR[file].head;
		int next = FAT[current];

		while(current != EOFF)//loops until frees the all the files
		{
			next = FAT[current];
			FAT[current] = 0;
			current = next;
		}
		DIR[file].ref_cnt--;// decrease the ref count
		files_open--; //decrease the dir count
		return 0;
	}
}

int fs_rdwr(int fd, void *dst, size_t nbyte,int mode)
{
	///errror checking
	if(nbyte < 1)
    	return -1;
  	if(fd < 0 || fd >= MAX_FILDES || fildes[fd].used == 0)
    {
    	fprintf(stderr,"fs_rdwr: bad filedescriptor\n");
      	return -1;
    }

     //find file from DIR[fd]
  	struct dir_entry *d = &DIR[fildes[fd].file];

  	//setting values
 	 off_t offs = fildes[fd].offset;
 	 int oldsize = d->size;
 
  	if(offs + nbyte > d->size)
  	{
   		if(mode == READ)//if mode is read 
			nbyte = d->size - offs;//decrease the offset
  	}

  	int counter = 0;
  	int numtoexpand;

  	if(mode == WRITE && offs+nbyte > d->size)//if the mode is wire and there arent enough space
   		numtoexpand = (offs+nbyte)/BLOCK_SIZE - oldsize/BLOCK_SIZE;
  	else
   		numtoexpand = d->size/BLOCK_SIZE - oldsize/BLOCK_SIZE;

  	int numblocks = (nbyte + (offs%BLOCK_SIZE))/BLOCK_SIZE + 1;//blocks to expand
  	int offsblock = offs/BLOCK_SIZE;//
  	int fatblock = d->head;
	char * buf = (char*)malloc(BLOCK_SIZE);//allocate a buffer with block size 
  	int i;
  	//load information from FAT
 	for(i = 0; i < offsblock; i++)
    {
      	if(fatblock < 0)//if fattblock is past
      	{
			fprintf(stderr,"fs_rdwr: trying to read past end of file\n"); 
			return -1;
   		}
      fatblock = FAT[fatblock];
    }

  	char *bufnum = (char *)dst;
  	int fsnum = offs % BLOCK_SIZE;
  	int dataamt;
  	int amountleft = nbyte;
  	for(i = 0; i < numblocks; i++)
	{
     
      	if(fsnum+amountleft > BLOCK_SIZE){//if there is an offset
			dataamt = BLOCK_SIZE - fsnum;
      	}
      	else{//else
			dataamt = amountleft;
     	}

     	
     	//if(
     	block_read(fatblock + 4096, buf);
     	//==-1)//cannot not read
		//{
		//	fprintf(stderr,"fs_rdwr: could not read from block");
	  	//	return -1;
		//}
      	if(mode == READ)//if mode is read
		{
			counter+= dataamt;
	 		memcpy(bufnum, buf+fsnum, dataamt);
		}
      	if(mode == WRITE)//if the mode is write
		{
	  		memcpy(buf+fsnum, bufnum, dataamt);//copy from bufnum to buf
	  		if(block_write(fatblock+4096, buf) == -1)
	    		break;

	  		counter+= dataamt;//add the data amount to the counter
		}
      	if(mode == READ)//if mode is read
		{
			counter+= dataamt;
	 		memcpy(bufnum, buf+fsnum, dataamt);
			}

    	if(i == (numblocks - numtoexpand -1) && numtoexpand > 0)//for writting when there are not enough space
		{
			int temp = get_free_FAT();
			if(temp == -1)
	  			return -1;

	  		FAT[fatblock] = temp;
	 		FAT[temp] = EOFF;
	  		numtoexpand--;
		}

   		 bufnum = bufnum + BLOCK_SIZE;
   		 fsnum = 0;//set the fsnum to 0

   		 fatblock = FAT[fatblock];
   		 amountleft = amountleft - dataamt;
    }
  	if(offs + counter > d->size)//if its past 
    {
      	if(mode == WRITE)//and you are writing with 
		{
	  	oldsize = d->size;
	  	d->size = offs + nbyte;
		}
    }

  fildes[fd].offset += counter;//add the counter to the offset
  return counter;
}

int fs_read(int fd, void *buf, size_t nbyte)
{

	return fs_rdwr(fd, buf, nbyte, READ);
}
int fs_write(int fd, void *buf, size_t nbyte)
{

	return fs_rdwr(fd, buf, nbyte, WRITE);
}

int fs_get_filesize(int fd)
{
	if(fd < 0 || fd >= MAX_FILDES || fildes[fd].used == 0)//error checkinh
    {
    	fprintf(stderr,"fs_get_filesize: some error in file descriptors\n");

      	return -1;
    }

  return DIR[fildes[fd].file].size;
}

int fs_listfiles(char ***files)
{
	if(files == NULL)//if there is no argument
	{
		fprintf(stderr,"fs_listfiles: no argument passed\n");
		return -1;
	}

	int i = 0;
	int cnt= 0;
	for( ; i <MAX_DIR; i++)//loops through all the files
	{
		if(DIR[i].used == 1)
		{
			files[cnt] = DIR[i].name;//copies the names to the right place in the array
			cnt++;
		}
	}

	files[cnt+1] = NULL;//sets the last eleement of the array to a NULL pointer

	return 0;
}

int fs_lseek(int fd, off_t offset)
{
	if(fd < 0 || fd >= MAX_FILDES || fildes[fd].used == 0)//error checking for fd
    {
    	fprintf(stderr,"fs_lseek: some error in file descriptors\n");

      	return -1;
    }
    if(offset < 0) // error checking for offset
   	{
   		fprintf(stderr,"fs_lseek: offset less than 0\n");
   		return -1;
   	}

   	fildes[fd].offset = offset;
   	return 0;
}

int fs_truncate(int fd, off_t length)
{
	////////////ERROR Checking
	if(fd < 0 || fd >= MAX_FILDES || fildes[fd].used == 0)//error checking for fd
    {
    	fprintf(stderr,"fs_truncate: some error in file descriptors\n");

      	return -1;
    }

    struct dir_entry *file = &DIR[fildes[fd].file];//get the file from the dir
    if(length > file->size)
    {
    	fprintf(stderr,"fs_truncate: length cant be bigger than file size\n");
    	return -1;
    }
    if(length < 0)
    {
    	fprintf(stderr,"fs_truncate: length cant be less than 0\n");
    	return -1;
    }

    /////////////Trancate////////////////
    int needed_block = length/BLOCK_SIZE; // get the blocks needed
    int current = file->head;
    int i = 0;
    for( ; i < needed_block; i++)//loop until you reach the requested size
	{
		current = FAT[current];
	} 

	int cutting = FAT[current];//to clear the rest of the files
	FAT[current] = EOFF;//set the end to EOF
	int next;
	while(cutting != EOFF)
	{
		next = FAT[cutting];//set the next
		FAT[cutting] = 0;//free the block
		cutting = next;//set the cutting to next
	}

	FAT[cutting] = 0; //clear the last Eof

	file->size = length;
	return 0;
}


