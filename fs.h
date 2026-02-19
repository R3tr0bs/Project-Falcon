#ifndef FS_H
#define FS_H

#include <stdint.h>
#include "heap.h"

#define FS_MAX_FILES 64
#define FS_MAX_FILENAME 32
#define FS_BLOCK_SIZE 512
#define FS_MAX_BLOCKS 1024

typedef struct fs_file {
    char name[FS_MAX_FILENAME];
    uint32_t size;
    uint32_t blocks[16];
    uint32_t block_count;
    uint8_t flags;
    uint32_t created;
    uint32_t modified;
} fs_file_t;

typedef struct fs_superblock {
    uint32_t magic;
    uint32_t version;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t total_files;
    uint32_t free_files;
} fs_superblock_t;

typedef struct fs {
    fs_superblock_t superblock;
    uint8_t* bitmap;
    fs_file_t* files;
    uint8_t* data_blocks;
} fs_t;

void fs_init(uint32_t start_addr, uint32_t size);
int fs_create(const char* name);
int fs_delete(const char* name);
int fs_write(const char* name, const void* data, uint32_t size);
int fs_read(const char* name, void* buffer, uint32_t size);
int fs_list(char* buffer, uint32_t max_size);
int fs_exists(const char* name);
uint32_t fs_get_size(const char* name);

#endif