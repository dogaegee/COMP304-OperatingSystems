#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <cassert>
#include <list>
#include <cstdlib>
#include <fstream>
#include <unistd.h>
#include "fat.h"
#include "fat_file.h"
#include <iostream>

/**
 * Write inside one block in the filesystem.
 * @param  fs           filesystem
 * @param  block_id     index of block in the filesystem
 * @param  block_offset offset inside the block
 * @param  size         size to write, must be less than BLOCK_SIZE
 * @param  buffer       data buffer
 * @return              written byte count
 */
int mini_fat_write_in_block(FAT_FILESYSTEM *fs, const int block_id, const int block_offset, const int size, const void * buffer) {
	assert(block_offset >= 0);
	assert(block_offset < fs->block_size);
	assert(size + block_offset <= fs->block_size);

	int written = 0;

	// TODO: write in the real file.

	return written;
}

/**
 * Read inside one block in the filesystem
 * @param  fs           filesystem
 * @param  block_id     index of block in the filesystem
 * @param  block_offset offset inside the block
 * @param  size         size to read, must fit inside the block
 * @param  buffer       buffer to write the read stuff to
 * @return              read byte count
 */
int mini_fat_read_in_block(FAT_FILESYSTEM *fs, const int block_id, const int block_offset, const int size, void * buffer) {
	assert(block_offset >= 0);
	assert(block_offset < fs->block_size);
	assert(size + block_offset <= fs->block_size);

	int read = 0;

	// TODO: read from the real file.
	
	return read;
}


/**
 * Find the first empty block in filesystem.
 * @return -1 on failure, index of block on success
 */
int mini_fat_find_empty_block(const FAT_FILESYSTEM *fat) {
	// TODO: find an empty block in fat and return its index.
	int i = 0;
	for (auto element: fat->block_map){
		if(element == 0){
			return i;
		}
		i++;
	}
	return -1;
}

/**
 * Find the first empty block in filesystem, and allocate it to a type,
 * i.e., set block_map[new_block_index] to the specified type.
 * @return -1 on failure, new_block_index on success
 */
int mini_fat_allocate_new_block(FAT_FILESYSTEM *fs, const unsigned char block_type) {
	int new_block_index = mini_fat_find_empty_block(fs);
	if (new_block_index == -1)
	{
		fprintf(stderr, "Cannot allocate block: filesystem is full.\n");
		return -1;
	}
	fs->block_map[new_block_index] = block_type;
	return new_block_index;
}

void mini_fat_dump(const FAT_FILESYSTEM *fat) {
	printf("Dumping fat with %d blocks of size %d:\n", fat->block_count, fat->block_size);
	for (int i=0; i<fat->block_count;++i) {
		printf("%d ", (int)fat->block_map[i]);
	}
	printf("\n");

	for (int i=0; i<fat->files.size(); ++i) {
		mini_file_dump(fat, fat->files[i]);
	}
}

static FAT_FILESYSTEM * mini_fat_create_internal(const char * filename, const int block_size, const int block_count) {
	FAT_FILESYSTEM * fat = new FAT_FILESYSTEM;
	fat->filename = filename;
	fat->block_size = block_size;
	fat->block_count = block_count;
	fat->block_map.resize(fat->block_count, EMPTY_BLOCK); // Set all blocks to empty.
	fat->block_map[0] = METADATA_BLOCK;
	return fat;
}

/**
 * Create a new virtual disk file.
 * The file should be of the exact size block_size * block_count bytes.
 * Overwrites existing files. Resizes block_map to block_count size.
 * @param  filename    name of the file on real disk
 * @param  block_size  size of each block
 * @param  block_count number of blocks
 * @return             FAT_FILESYSTEM pointer with parameters set.
 */
FAT_FILESYSTEM * mini_fat_create(const char * filename, const int block_size, const int block_count) {

	FAT_FILESYSTEM * fat = mini_fat_create_internal(filename, block_size, block_count);
	// TODO: create the corresponding virtual disk file with appropriate size.
	int retVal;
	std::ofstream ofs(filename, std::ios::out);
    	ofs.seekp((block_size*block_count) - 1);
    	ofs.write("", 1);
	return fat;
}

/**
 * Save a virtual disk (filesystem) to file on real disk.
 * Stores filesystem metadata (e.g., block_size, block_count, block_map, etc.)
 * in block 0.
 * Stores file metadata (name, size, block map) in their corresponding blocks.
 * Does not store file data (they are written directly via write API).
 * @param  fat virtual disk filesystem
 * @return     true on success
 */
bool mini_fat_save(const FAT_FILESYSTEM *fat) {
	FILE * fat_fd = fopen(fat->filename, "r+");
	if (fat_fd == NULL) {
		perror("Cannot save fat to file");
		return false;
	}
	// TODO: save all metadata (filesystem metadata, file metadata).
	std::string bc = std::to_string(fat->block_count);
	std::string bs = std::to_string(fat->block_size);
	std::vector <unsigned char> bm = fat->block_map;
	std::string filesystem_metadata_str = bc + " " + bs + " ";
	for(int i = 0; i< bm.size(); i++){
		filesystem_metadata_str.append(std::to_string(bm[i]));
		filesystem_metadata_str.append(" ");		
	}
	
	fwrite(filesystem_metadata_str.c_str(), sizeof(char), sizeof(filesystem_metadata_str), fat_fd);
	
	std::vector <FAT_FILE*> files = fat->files;
	std::string save_str;
	for(int i=0; i< files.size(); i++){
		int metadata_bid = files[i]->metadata_block_id;
		std::string size = std::to_string(files[i]->size);
		std::string name_size = std::to_string(strlen(files[i]->name));
		std::string name = files[i]->name;
		save_str = size + " " + name_size + " " + name + " ";
		for(int j= 0; j < files[i]->block_ids.size(); j++){
			std::string id_size = std::to_string(files[i]->block_ids[j]);
			(save_str).append(id_size);
			(save_str).append(" ");
		}
		save_str.append("\0");
		fseek(fat_fd, metadata_bid*(fat->block_size), SEEK_SET);
		char * save_char = (char *) (save_str).c_str();  
		*(save_char+strlen(save_char)) = '\0';
		fwrite(save_char, sizeof(char), strlen(save_char), fat_fd);
	}

	fclose(fat_fd);

	return true;
}

FAT_FILESYSTEM * mini_fat_load(const char *filename) {
	FILE * fat_fd = fopen(filename, "r+");
	if (fat_fd == NULL) {
		perror("Cannot load fat from file");
		exit(-1);
	}
	// TODO: load all metadata (filesystem metadata, file metadata) and create filesystem.
	
	int block_size = 1024, block_count = 10;
	FAT_FILESYSTEM * fat = mini_fat_create_internal(filename, block_size, block_count);
	
	char * metadata = (char *) malloc(sizeof(char) * block_size);
	fread(metadata, sizeof(char), block_size, fat_fd);
	metadata[block_size] = '\0';
	char * metadata_token = strtok(metadata, " ");
	int metadata_counter = 1;
	int index = 0;
	fat->block_map.resize(block_count, EMPTY_BLOCK);
	
	while(metadata_token != NULL){
		metadata_token = strtok(NULL, " ");
		if(metadata_token == NULL){
			break;
		}
		metadata_counter++;
		if(metadata_counter >= 3){
			int k = atoi(metadata_token);
			fat->block_map[index] = k;
			index++;
		}
	}
	int map_size = fat->block_map.size();
	metadata_counter = 0;
	
	free(metadata);
	char * metadata_file = (char *) malloc(sizeof(char) * block_size);
	for(int m = 0; m < map_size; m++){
		int map = fat->block_map[m];
		if(map == 1){
			FAT_FILE * file = new FAT_FILE;
			fat->files.push_back(file);
			fseek(fat_fd, m*block_size, SEEK_SET);
			fread(metadata_file, sizeof(char), block_size, fat_fd);
			metadata_file[block_size] = '\0';
			metadata_token = strtok(metadata_file, " ");
			int size = atoi(metadata_token);
			file->size = size;
			int name_length;
			metadata_token = strtok(NULL, " ");
			name_length = atoi(metadata_token);
			metadata_token = strtok(NULL, " ");
			strncpy(file->name, metadata_token, name_length);
			file->name[name_length] = '\0';
			file->metadata_block_id = m;
			while(metadata_token != NULL){
				metadata_token = strtok(NULL, " ");
				if(metadata_token == NULL){
					break;
				}
				int block_index = atoi(metadata_token);
				file->block_ids.push_back(block_index);
			}
		}
	}
	free(metadata_file);
	fclose(fat_fd);
	
	return fat;
}
