#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

/* RAID 5 */
unsigned int			chunk_size = 512 * 1024;	// 512KB
unsigned int			super_offset = 8 * 512;		// 8 sectors
unsigned int			data_offset = 2048 * 512;	// 2048 sectors

unsigned long long		raid_size = 3838825472ULL << 9ULL;

int				drive_fd[3] = {-1,-1,-1};

static const char *hello_path = "/data";

static int hello_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (strcmp(path, hello_path) == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = raid_size;
	} else
		res = -ENOENT;

	return res;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, hello_path + 1, NULL, 0);

	return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
	if (strcmp(path, hello_path) != 0)
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	(void) fi;

	if (strcmp(path, hello_path) != 0)
		return -ENOENT;

	return 0;
}

static struct fuse_operations hello_oper = {
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.open		= hello_open,
	.read		= hello_read,
};

int main(int argc, char *argv[])
{
	drive_fd[0] = open("/dev/sda5",O_RDONLY);
	if (drive_fd[0] < 0) {
		fprintf(stderr,"Cannot open drive 1: %s\n",strerror(errno));
		return 1;
	}
	drive_fd[1] = open("/dev/sdb5",O_RDONLY);
	if (drive_fd[0] < 0) {
		fprintf(stderr,"Cannot open drive 2: %s\n",strerror(errno));
		return 1;
	}
	drive_fd[2] = open("/dev/sdc5",O_RDONLY);
	if (drive_fd[0] < 0) {
		fprintf(stderr,"Cannot open drive 3: %s\n",strerror(errno));
		return 1;
	}

	return fuse_main(argc, argv, &hello_oper, NULL);
}
