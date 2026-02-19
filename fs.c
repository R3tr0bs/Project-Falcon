#include "fs.h"
#include "utils.h"

#define FS_MAGIC 0x46414C43 // "FALC"
#define FS_VERSION 1

static fs_t fs;

static int find_free_block(void) {
    for (uint32_t i = 0; i < fs.superblock.total_blocks; i++) {
        if ((fs.bitmap[i / 8] & (1 << (i % 8))) == 0) {
            return i;
        }
    }
    return -1;
}

static void set_block_used(uint32_t block) {
    fs.bitmap[block / 8] |= (1 << (block % 8));
    fs.superblock.free_blocks--;
}

static void set_block_free(uint32_t block) {
    fs.bitmap[block / 8] &= ~(1 << (block % 8));
    fs.superblock.free_blocks++;
}

static int find_free_file(void) {
    for (uint32_t i = 0; i < fs.superblock.total_files; i++) {
        if (fs.files[i].name[0] == '\0') {
            return i;
        }
    }
    return -1;
}

static int find_file(const char* name) {
    for (uint32_t i = 0; i < fs.superblock.total_files; i++) {
        if (fs.files[i].name[0] != '\0') {
            int j;
            for (j = 0; name[j] && fs.files[i].name[j]; j++) {
                if (name[j] != fs.files[i].name[j]) break;
            }
            if (name[j] == '\0' && fs.files[i].name[j] == '\0') {
                return i;
            }
        }
    }
    return -1;
}

void fs_init(uint32_t start_addr, uint32_t size) {
    uint32_t bitmap_size = (FS_MAX_BLOCKS + 7) / 8;
    uint32_t files_size = sizeof(fs_file_t) * FS_MAX_FILES;
    uint32_t data_size = FS_MAX_BLOCKS * FS_BLOCK_SIZE;
    uint32_t total_size = sizeof(fs_superblock_t) + bitmap_size + files_size + data_size;
    
    if (size < total_size) {
        return;
    }
    
    uint8_t* ptr = (uint8_t*)start_addr;
    
    fs.superblock.magic = FS_MAGIC;
    fs.superblock.version = FS_VERSION;
    fs.superblock.total_blocks = FS_MAX_BLOCKS;
    fs.superblock.free_blocks = FS_MAX_BLOCKS;
    fs.superblock.total_files = FS_MAX_FILES;
    fs.superblock.free_files = FS_MAX_FILES;
    
    ptr += sizeof(fs_superblock_t);
    fs.bitmap = ptr;
    for (uint32_t i = 0; i < bitmap_size; i++) {
        fs.bitmap[i] = 0;
    }
    
    ptr += bitmap_size;
    fs.files = (fs_file_t*)ptr;
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        fs.files[i].name[0] = '\0';
        fs.files[i].size = 0;
        fs.files[i].block_count = 0;
        fs.files[i].flags = 0;
        fs.files[i].created = 0;
        fs.files[i].modified = 0;
    }
    
    ptr += files_size;
    fs.data_blocks = ptr;
}

int fs_create(const char* name) {
    if (!name || name[0] == '\0') return -1;
    
    int file_idx = find_file(name);
    if (file_idx >= 0) return -1;
    
    file_idx = find_free_file();
    if (file_idx < 0) return -1;
    
    fs_file_t* file = &fs.files[file_idx];
    
    int i;
    for (i = 0; name[i] && i < FS_MAX_FILENAME - 1; i++) {
        file->name[i] = name[i];
    }
    file->name[i] = '\0';
    
    file->size = 0;
    file->block_count = 0;
    file->flags = 0;
    file->created = 0;
    file->modified = 0;
    
    fs.superblock.free_files--;
    
    return 0;
}

int fs_delete(const char* name) {
    if (!name || name[0] == '\0') return -1;
    
    int file_idx = find_file(name);
    if (file_idx < 0) return -1;
    
    fs_file_t* file = &fs.files[file_idx];
    
    for (uint32_t i = 0; i < file->block_count; i++) {
        set_block_free(file->blocks[i]);
    }
    
    file->name[0] = '\0';
    file->size = 0;
    file->block_count = 0;
    
    fs.superblock.free_files++;
    
    return 0;
}

int fs_write(const char* name, const void* data, uint32_t size) {
    if (!name || !data || size == 0) return -1;
    
    int file_idx = find_file(name);
    if (file_idx < 0) return -1;
    
    fs_file_t* file = &fs.files[file_idx];
    
    for (uint32_t i = 0; i < file->block_count; i++) {
        set_block_free(file->blocks[i]);
    }
    
    file->block_count = 0;
    uint32_t blocks_needed = (size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    
    if (blocks_needed > 16) return -1;
    
    for (uint32_t i = 0; i < blocks_needed; i++) {
        int block_idx = find_free_block();
        if (block_idx < 0) {
            for (uint32_t j = 0; j < file->block_count; j++) {
                set_block_free(file->blocks[j]);
            }
            file->block_count = 0;
            return -1;
        }
        
        file->blocks[file->block_count] = block_idx;
        set_block_used(block_idx);
        file->block_count++;
    }
    
    uint32_t offset = 0;
    for (uint32_t i = 0; i < file->block_count; i++) {
        uint32_t copy_size = (size - offset > FS_BLOCK_SIZE) ? FS_BLOCK_SIZE : (size - offset);
        uint8_t* block_ptr = fs.data_blocks + (file->blocks[i] * FS_BLOCK_SIZE);
        const uint8_t* data_ptr = (const uint8_t*)data + offset;
        
        for (uint32_t j = 0; j < copy_size; j++) {
            block_ptr[j] = data_ptr[j];
        }
        
        offset += copy_size;
    }
    
    file->size = size;
    file->modified = 0;
    
    return 0;
}

int fs_read(const char* name, void* buffer, uint32_t size) {
    if (!name || !buffer) return -1;
    
    int file_idx = find_file(name);
    if (file_idx < 0) return -1;
    
    fs_file_t* file = &fs.files[file_idx];
    
    uint32_t read_size = (size > file->size) ? file->size : size;
    
    uint32_t offset = 0;
    for (uint32_t i = 0; i < file->block_count && offset < read_size; i++) {
        uint32_t copy_size = (read_size - offset > FS_BLOCK_SIZE) ? FS_BLOCK_SIZE : (read_size - offset);
        uint8_t* block_ptr = fs.data_blocks + (file->blocks[i] * FS_BLOCK_SIZE);
        uint8_t* buffer_ptr = (uint8_t*)buffer + offset;
        
        for (uint32_t j = 0; j < copy_size; j++) {
            buffer_ptr[j] = block_ptr[j];
        }
        
        offset += copy_size;
    }
    
    return read_size;
}

int fs_list(char* buffer, uint32_t max_size) {
    if (!buffer || max_size == 0) return -1;
    
    uint32_t offset = 0;
    
    for (uint32_t i = 0; i < fs.superblock.total_files; i++) {
        if (fs.files[i].name[0] != '\0') {
            int name_len = 0;
            while (fs.files[i].name[name_len]) name_len++;
            
            if (offset + name_len + 16 >= max_size) break;
            
            for (int j = 0; fs.files[i].name[j]; j++) {
                buffer[offset++] = fs.files[i].name[j];
            }
            
            buffer[offset++] = ' ';
            buffer[offset++] = '(';
            
            uint32_t size = fs.files[i].size;
            char size_buf[16];
            int size_len = 0;
            
            if (size == 0) {
                size_buf[size_len++] = '0';
            } else {
                while (size > 0) {
                    size_buf[size_len++] = (size % 10) + '0';
                    size /= 10;
                }
                for (int j = 0; j < size_len / 2; j++) {
                    char temp = size_buf[j];
                    size_buf[j] = size_buf[size_len - j - 1];
                    size_buf[size_len - j - 1] = temp;
                }
            }
            
            for (int j = 0; j < size_len; j++) {
                buffer[offset++] = size_buf[j];
            }
            
            buffer[offset++] = ' ';
            buffer[offset++] = 'B';
            buffer[offset++] = ')';
            buffer[offset++] = '\n';
        }
    }
    
    buffer[offset] = '\0';
    return offset;
}

int fs_exists(const char* name) {
    return find_file(name) >= 0;
}

uint32_t fs_get_size(const char* name) {
    int file_idx = find_file(name);
    if (file_idx < 0) return 0;
    return fs.files[file_idx].size;
}