#include "bufferToolDisk.h"
#include "js2.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <ftw.h>
#include <libgen.h>

//#define SLOW_DISK
#define SLOW_DISK_WAIT 100000

// mkpath() shamelessly stolen from
// https://stackoverflow.com/questions/2336242/recursive-mkdir-system-call-on-unix
//int mkpath(char *dir, mode_t mode);
int mkpath(char *dir, mode_t mode)
{
    struct stat sb;

    if (!dir) {
        errno = EINVAL;
        return 1;
    }

    if (!stat(dir, &sb))
        return 0;

    mkpath(dirname(strdupa(dir)), mode);

    return mkdir(dir, mode);
}

// rmTree() shamelessly stolen from
// https://stackoverflow.com/questions/2256945/removing-a-non-empty-directory-programmatically-in-c-or-c
static int rmFiles(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb) {
    if (remove(pathname) < 0) {
        perror("ERROR: rmTree() remove");
        return -1;
    }
    return 0;
}

int rmTree(char *path) {
    // Delete the directory and its contents by traversing the tree in reverse order, without
    // crossing mount boundaries and symbolic links
    if (nftw(path, rmFiles,10, FTW_DEPTH|FTW_MOUNT|FTW_PHYS) < 0) {
        perror("ERROR: rmTree() ntfw");
        // exit(1);
    }

    return 1;
}

int SimpleDiskBuffer::setup(const char *_path) {
    path = strdup(_path);

    mkpath(path, 0777);

    // 128 bytes is more than enough space for my mangling of a BufferId
    nameAlloc = strlen(path) + 128;
    return 1;
}


void SimpleDiskBuffer::clear() {
    rmTree(path);
}

#define BuildName  char *name = (char *)alloca(nameAlloc); \
                   sprintf(name, "%s/buf_%u_%u_%u_%u_%u.dat", path, bId.stepGroup, \
	           bId.step, bId.mergeLevel, bId.group, bId.buf)


void SimpleDiskBuffer::writeBuffer(BufferId bId, uint8_t *buf, size_t len) {
    BuildName;

    #ifdef SLOW_DISK
    usleep(SLOW_DISK_WAIT);
    #endif
    
    int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    test(fd != -1, "Can't create simple disk buffer");
    size_t wrLen = write(fd, buf, len);
    test(len == wrLen, "failed to write simple disk buffer");
    close(fd);
}

uint8_t *SimpleDiskBuffer::fetchBuffer(BufferId bId, size_t len) {
    BuildName;

    #ifdef SLOW_DISK
    usleep(SLOW_DISK_WAIT);
    #endif
    
    uint8_t *buf = (uint8_t *)malloc(len);
    test(buf != NULL, "can't malloc space for simple disk buffer");
    int fd = open(name, O_RDONLY);
    test(fd != -1, "Can't open simple disk buffer");
    size_t rdLen = read(fd, buf, len);
    test (rdLen == len, "can't read full simple disk buffer");
    close(fd);

    return buf;
}

void SimpleDiskBuffer::releaseBuffer(uint8_t *buf, size_t len) {
    free(buf);
}

void SimpleDiskBuffer::deleteBuffer(BufferId bId) {
    BuildName;

    #ifdef SLOW_DISK
    usleep(SLOW_DISK_WAIT);
    #endif
    
    if (remove(name) < 0)
	perror("remove error in deleteBuffer()");
}
