#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

/* RAID 5 */
unsigned int			chunk_size = 512 * 1024;	// 512KB
unsigned int			super_offset = 8 * 512;		// 8 sectors
unsigned int			data_offset = 2048 * 512;	// 2048 sectors

unsigned long long		raid_size = 3838825472ULL << 9ULL;

unsigned int			number_of_drives = 3;

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
	size_t stripe_size = (size_t)chunk_size * (size_t)number_of_drives;
	int rd = 0,total = 0;
	unsigned char *chunk;
	(void) fi;

	if (strcmp(path, hello_path) != 0)
		return -ENOENT;

	/*  A1   A2   Ap  =>  0  1  p
	 *  B2   Bp   B1  =>  3  p  2
	 *  Cp   C1   C2  =>  p  4  5
	 *  D1   D2   Dp  =>  6  7  p
	 *  E2   Ep   E1  =>  9  p  8
	 *  Fp   F1   F2  =>  p 10 11  */

	unsigned char *stripe = malloc(stripe_size);
	if (stripe == NULL) return -ENOMEM;

	while (size > 0) {
		off_t chunk_offset = offset % (off_t)chunk_size;
		off_t chunk_number = offset / (off_t)chunk_size;
		off_t stripe_number = chunk_number / (off_t)(number_of_drives - 1);
		off_t stripe_chunk_sel = chunk_number % (off_t)(number_of_drives - 1);
		off_t stripe_chunk_offset = stripe_number * (off_t)chunk_size;
		unsigned int raid5_rot = (unsigned int)(stripe_number % (off_t)number_of_drives);
		unsigned int disk_sel = (((unsigned int)stripe_chunk_sel + (unsigned int)number_of_drives - (unsigned int)raid5_rot)) % (unsigned int)number_of_drives;

		chunk = stripe + ((size_t)disk_sel * (size_t)chunk_size);

		size_t cando = (size_t)chunk_size - chunk_offset;
		if (cando > size) cando = size;

		fprintf(stderr,"chunk=%llu offset=%llu stripe=%llu sel=%llu stripeoffset=%llu raid5rot=%u disk=%u read=%zu\n",
			(unsigned long long)chunk_number,
			(unsigned long long)chunk_offset,
			(unsigned long long)stripe_number,
			(unsigned long long)stripe_chunk_sel,
			(unsigned long long)stripe_chunk_offset,
			(unsigned int)raid5_rot,
			(unsigned int)disk_sel,
			cando);

		/* factor in data offset */
		stripe_chunk_offset += (off_t)data_offset;

		if (lseek(drive_fd[disk_sel],stripe_chunk_offset,SEEK_SET) != stripe_chunk_offset) {
			fprintf(stderr,"lseek fail, drive %u\n",disk_sel);
			break;
		}

		memset(chunk,0,chunk_size);
		rd = read(drive_fd[disk_sel],chunk,chunk_size);
		if (rd < chunk_size)
			fprintf(stderr,"read error, drive %u\n",disk_sel);

		assert((chunk_offset+cando) <= (size_t)chunk_size);
		memcpy(buf,chunk+chunk_offset,cando);

		buf += cando;
		size -= cando;
		total += cando;
		offset += (off_t)cando;
	}

	free(stripe);
	return total;
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
