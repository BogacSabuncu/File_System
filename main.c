#include <stdio.h>
#include <memory.h>
#include "disk.h"
//#include "fs.c"
#define SIZE 4000

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


void read_and_print(int fd, char *buf2, int size, int ret)
{
	memset(buf2, '0', 2*SIZE);
	ret = fs_read(fd, buf2, size);
	printf("fs_read:\t%d\n", ret);
	int i = 0;
	for(i = 0; i < 10; i++)
	{
		printf("%c", buf2[i]);
	}
	printf("\n");
}


int main(int argc, char **argv)
{
	
	int fd;
	char buf[2*SIZE];
	int ret = 999;
	char *d_name0 = "disk.0";
	char *d_name1 = "disk.1";
	char *d_name2 = "disk.2";
	ret = make_disk(d_name2);
	printf("%d\n", ret);
	ret = make_disk(d_name2);
	printf("%d\n", ret); // the result means disk will be overwriten if using the same name
	printf("1.test make_fs\n");
	ret = make_fs(d_name0);
	printf("make_fs 1:\t%d\n", ret);
	ret = make_fs(d_name0);
	printf("make_fs 2:\t%d\n", ret);
	printf("\n2.test mount_fs\n");
	ret = mount_fs("lala");
	printf("mount_fs 1:\t%d\n\n", ret);
	ret = mount_fs(d_name0);
	printf("mount_fs 2:\t%d\n\n", ret);
	ret = mount_fs(d_name0);
	printf("mount_fs 3:\t%d\n\n", ret);
	printf("\n3.test fs_create\n");
	char *f_name0 = "file.0";
	ret = fs_create(f_name0);
	printf("fs_create 1:\t%d\n", ret);
	ret = fs_create(f_name0);
	printf("fs_create 2:\t%d\n", ret);
	printf("\n4.test fs_open\n");
	char *f_name1 = "file.1";
	ret = fs_open(f_name1);
	printf("fs_open 1:\t%d\n", ret);
	ret = fs_open(f_name0);
	printf("fs_open 2:\t%d\n", ret);
	int fd0 = 999;
	int fd1 = ret;
	printf("\n5.test fs_write\n");
	memset(buf, 'a', 2*SIZE);
	ret = fs_write(fd0, buf, SIZE);
	printf("fs_write 0:\t%d\n", ret);
	ret = fs_write(fd1, buf, SIZE);
	printf("fs_write 1:\t%d\n", ret);
	memset(buf, 'b', 2*SIZE);
	ret = fs_write(fd1, buf, SIZE);
	printf("fs_write 2:\t%d\n", ret);
	char buf2[2*SIZE];
	printf("\n6.test fs_lseek\n");
	ret = fs_lseek(fd0, 1);
	printf("fs_lseek 0:\t%d\n", ret);
	ret = fs_lseek(fd1, 1);
	printf("fs_lseek 1:\t%d\n", ret);
	read_and_print(fd1, buf2, SIZE, ret);
	ret = fs_lseek(fd1, -1);
	printf("fs_lseek 2:\t%d\n", ret);
	ret = fs_lseek(fd1, SIZE+1);
	printf("fs_lseek 3:\t%d\n", ret);
	read_and_print(fd1, buf2, SIZE, ret);
	ret = fs_lseek(fd1, 0);
	printf("fs_lseek 4:\t%d\n", ret);
	printf("\n7.test fs_read\n");
	read_and_print(fd0, buf2, SIZE, ret);
	read_and_print(fd1, buf2, SIZE, ret);
	read_and_print(fd1, buf2, SIZE, ret);
	printf("\n8.test fs_get_filesize\n");
	ret = fs_get_filesize(fd0);
	printf("fs_get_filesize 0:\t%d\n", ret);
	ret = fs_get_filesize(fd1);
	printf("fs_get_filesize 1:\t%d\n", ret);
	printf("\n9.test fs_truncate\n");
	ret = fs_truncate(fd0, 3);
	printf("fs_truncate 0:\t%d\n", ret);
	ret = fs_truncate(fd1, 3);
	printf("fs_truncate 1:\t%d\n", ret);
	ret = fs_lseek(fd1, 0);
	read_and_print(fd1, buf2, 3, ret);
	printf("\n10.test fs_delete\n");
	ret = fs_delete(f_name0);
	printf("fs_delete 0:\t%d\n", ret);
	ret = fs_delete(f_name1);
	printf("fs_delete 1:\t%d\n", ret);
	printf("\n11.test fs_close\n");
	ret = fs_close(fd0);
	printf("fs_close 0:\t%d\n", ret);
	ret = fs_close(fd1);
	printf("fs_close 1:\t%d\n", ret);
	ret = fs_delete(f_name0);
	printf("fs_delete 1:\t%d\n", ret);
	ret = fs_create(f_name1);
	printf("\ncreate unempty file1 before umount:\t%d", ret);
	ret = fs_open(f_name1);
	fd1 = ret;
	memset(buf, 'A', 2*SIZE);
	ret = fs_write(fd1, buf, SIZE);
	ret = fs_lseek(fd1, 0);
	ret = fs_close(fd1);
	printf("\n12.test umount_fs\n");
	ret = umount_fs(d_name1);
	printf("umount 0:\t%d\n", ret);
	ret = umount_fs(d_name0);
	printf("umount 1:\t%d\n", ret);
	ret = mount_fs(d_name0);
	ret = fs_open(f_name1);
	fd1 = ret;
	read_and_print(fd1, buf2, SIZE, ret);
	ret = umount_fs(d_name0); 


}
