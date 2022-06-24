#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <iostream>
#include <fstream>
#include <bitset>
#include "fat.h"
#include "fat_file.h"

// Little helper to show debug messages. Set 1 to 0 to silence.
#define DEBUG 1
#define DELETED_METADATA -1

int to_binary(int decimal);
inline void debug(const char * fmt, ...) {
#if DEBUG>0
	va_list args;
   va_start(args, fmt);
   vprintf(fmt, args);
   va_end(args);
#endif
}

// Delete index-th item from vector.
template<typename T>
static void vector_delete_index(std::vector<T> &vector, const int index) {
	vector.erase(vector.begin() + index);
}

// Find var and delete from vector.
template<typename T>
static bool vector_delete_value(std::vector<T> &vector, const T var) {
	for (int i=0; i<vector.size(); ++i) {
		if (vector[i] == var) {
			vector_delete_index(vector, i);
			return true;
		}
	}
	return false;
}

void mini_file_dump(const FAT_FILESYSTEM *fs, const FAT_FILE *file)
{
	printf("Filename: %s\tFilesize: %d\tBlock count: %d\n", file->name, file->size, (int)file->block_ids.size());
	printf("\tMetadata block: %d\n", file->metadata_block_id);
	printf("\tBlock list: ");
	for (int i=0; i<file->block_ids.size(); ++i) {
		printf("%d ", file->block_ids[i]);
	}
	printf("\n");

	printf("\tOpen handles: \n");
	for (int i=0; i<file->open_handles.size(); ++i) {
		printf("\t\t%d) Position: %d (Block %d, Byte %d), Is Write: %d\n", i,
			file->open_handles[i]->position,
			position_to_block_index(fs, file->open_handles[i]->position),
			position_to_byte_index(fs, file->open_handles[i]->position),
			file->open_handles[i]->is_write);
	}
}


/**
 * Find a file in loaded filesystem, or return NULL.
 */
FAT_FILE * mini_file_find(const FAT_FILESYSTEM *fs, const char *filename)
{
	for (int i=0; i<fs->files.size(); ++i) {
		if (strcmp(fs->files[i]->name, filename) == 0) // Match
			return fs->files[i];
	}
	return NULL;
}

/**
 * Create a FAT_FILE struct and set its name.
 */
FAT_FILE * mini_file_create(const char * filename)
{
	FAT_FILE * file = new FAT_FILE;
	file->size = 0;
	strcpy(file->name, filename);
	return file;
}


/**
 * Create a file and attach it to filesystem.
 * @return FAT_OPEN_FILE pointer on success, NULL on failure
 */
FAT_FILE * mini_file_create_file(FAT_FILESYSTEM *fs, const char *filename)
{
	assert(strlen(filename)< MAX_FILENAME_LENGTH);
	FAT_FILE *fd = mini_file_create(filename);

	int new_block_index = mini_fat_allocate_new_block(fs, FILE_ENTRY_BLOCK);
	if (new_block_index == -1)
	{
		fprintf(stderr, "Cannot create new file '%s': filesystem is full.\n", filename);
		return NULL;
	}
	fs->files.push_back(fd); // Add to filesystem.
	fd->metadata_block_id = new_block_index;
	return fd;
}

/**
 * Return filesize of a file.
 * @param  fs       filesystem
 * @param  filename name of file
 * @return          file size in bytes, or zero if file does not exist.
 */
int mini_file_size(FAT_FILESYSTEM *fs, const char *filename) {
	FAT_FILE * fd = mini_file_find(fs, filename);
	if (!fd) {
		fprintf(stderr, "File '%s' does not exist.\n", filename);
		return 0;
	}
	return fd->size;
}


/**
 * Opens a file in filesystem.
 * If the file does not exist, returns NULL, unless it is write mode, where
 * the file is created.
 * Adds the opened file to file's open handles.
 * @param  is_write whether it is opened in write (append) mode or read.
 * @return FAT_OPEN_FILE pointer on success, NULL on failure
 */
FAT_OPEN_FILE * mini_file_open(FAT_FILESYSTEM *fs, const char *filename, const bool is_write)
{
	FAT_FILE * fd = mini_file_find(fs, filename);
	if (!fd) {
		// TODO: check if it's write mode, and if so create it. Otherwise return NULL.
		if(is_write) {
				fd  = mini_file_create_file(fs, filename);
				if(fd == NULL){
					return NULL;
				}
		} else {
			return NULL;
		}
	}
	
	if (is_write) {
		// TODO: check if other write handles are open.
		for(auto element : fd->open_handles){
			if(element->is_write){
				return NULL;
			}
		}
	}
	
	
	FAT_OPEN_FILE * open_file = new FAT_OPEN_FILE;
	// TODO: assign open_file fields.
	open_file->file = fd;
	if(fd->block_ids.empty()){
		open_file->position = 0;
	}
	open_file->is_write = is_write;
	// Add to list of open handles for fd:
	fd->open_handles.push_back(open_file);
	return open_file;
}

/**
 * Close an existing open file handle.
 * @return false on failure (no open file handle), true on success.
 */
bool mini_file_close(FAT_FILESYSTEM *fs, const FAT_OPEN_FILE * open_file)
{
	if (open_file == NULL) return false;
	FAT_FILE * fd = open_file->file;
	if (vector_delete_value(fd->open_handles, open_file)) {
		return true;
	}

	fprintf(stderr, "Attempting to close file that is not open.\n");
	return false;
}

/**
 * Write size bytes from buffer to open_file, at current position.
 * @return           number of bytes written.
 */
int mini_file_write(FAT_FILESYSTEM *fs, FAT_OPEN_FILE * open_file, const int size, const void * buffer)
{
	int written_bytes = 0;

	// TODO: write to file.	
	FAT_FILE * file = open_file->file;
	const char* etherhead = (const char*)buffer;
	int buffersize = size;
	int block_size = fs->block_size;
	int current_block_location = open_file->position % block_size;
	int remaining_place = block_size - current_block_location;
	
	if(file->block_ids.empty()){
		int empty_id = mini_fat_find_empty_block(fs);
		printf("\nEMPTY ID:%d\n", empty_id);
		fs->block_map[empty_id] = FILE_DATA_BLOCK;
		file->block_ids.push_back(empty_id);
		if(remaining_place >= buffersize){
			FILE *fp;
			fp = fopen(fs->filename, "rb+");
			fseek(fp, empty_id*block_size+current_block_location, SEEK_SET);
			const long sss = fwrite(etherhead, sizeof(char), buffersize, fp);
			written_bytes += buffersize;
			file->size += buffersize;
			open_file->position += buffersize;
			fclose(fp);
		} else {
			FILE *fp;
			fp = fopen(fs->filename, "rb+");
			fseek(fp, empty_id*block_size+current_block_location, SEEK_SET);
			const long sss = fwrite(etherhead, sizeof(char), remaining_place, fp);
			written_bytes += remaining_place;
			file->size += remaining_place;
			open_file->position += remaining_place;
			buffersize -= remaining_place;
			etherhead += remaining_place;
			while(buffersize){
				empty_id = mini_fat_find_empty_block(fs);
				fs->block_map[empty_id] = FILE_DATA_BLOCK;
				file->block_ids.push_back(empty_id);
				fseek(fp, empty_id*block_size+current_block_location, SEEK_SET);
				if(buffersize <= block_size){
					const long sss = fwrite(etherhead, sizeof(char), buffersize, fp);
					written_bytes += buffersize;
					file->size += buffersize;
					open_file->position += buffersize;
					buffersize -= buffersize;
					etherhead += buffersize;
					break;
				} else {
					const long sss = fwrite(etherhead, sizeof(char), block_size, fp);
					written_bytes += block_size;
					file->size += block_size;
					open_file->position += block_size;
					buffersize -= block_size;
					etherhead += block_size;
				}
		
			}
			fclose(fp);
			
		}
		
		
	} else {
		int last_written_block = file->block_ids.back();
		int current_block = open_file->position / block_size;
		int remaining_place = block_size - current_block_location;
		if(remaining_place >= buffersize){
				FILE *fp;
				fp = fopen(fs->filename, "rb+");
				fseek(fp, file->block_ids[current_block]*block_size+current_block_location, SEEK_SET);
				fwrite(etherhead, sizeof(char), buffersize, fp);
				written_bytes += buffersize;
				file->size += buffersize;
				open_file->position += buffersize;
				fclose(fp);
		} else {
			FILE *fp;
			fp = fopen(fs->filename, "rb+");
			fseek(fp, file->block_ids[current_block]*block_size+current_block_location, SEEK_SET);
			fwrite(etherhead, sizeof(char), remaining_place, fp);
			written_bytes += remaining_place;
			file->size += remaining_place;
			open_file->position += remaining_place;
			buffersize -= remaining_place;
			etherhead += remaining_place;
			while(buffersize){
				int empty_id = mini_fat_find_empty_block(fs);
				fs->block_map[empty_id] = FILE_DATA_BLOCK;
				file->block_ids.push_back(empty_id);
				fseek(fp, empty_id*block_size, SEEK_SET);
				if(buffersize <= block_size){
					fwrite(etherhead, sizeof(char), buffersize, fp);
					written_bytes += buffersize;
					file->size += buffersize;
					open_file->position += buffersize;
					buffersize -= buffersize;
					etherhead += buffersize;
					break;
				} else {
					fwrite(etherhead, sizeof(char), block_size, fp);
					written_bytes += block_size;
					file->size += block_size;
					open_file->position += block_size;
					buffersize -= block_size;
					etherhead += block_size;
				}
			}
			fclose(fp);
		}
	}
	

	
	return written_bytes;
}

/**
 * Read up to size bytes from open_file into buffer.
 * @return           number of bytes read.
 */
int mini_file_read(FAT_FILESYSTEM *fs, FAT_OPEN_FILE * open_file, const int size, void * buffer)
{
	int read_bytes = 0;

	// TODO: read file.
	FILE *fp;
	fp = fopen(fs->filename, "rb");
	FAT_FILE * file = open_file->file;
	int file_size = file->size;
	std::vector<int> block_ids = file->block_ids;
	int block_size = fs->block_size;
	int current_block_location = open_file->position;
	int remaining_place = block_size - current_block_location;
	int read_size = size;
	for(int i = 0; i < block_ids.size(); i++){
		int current_id = block_ids[i];
		
		if(i == block_ids.size() - 1){
			int remainder = block_size;
			file_size = file_size - read_bytes - open_file->position;
			if(read_size <= file_size){
				fseek(fp, current_id*block_size+open_file->position, SEEK_SET);
				const long sss = fread(buffer + read_bytes, sizeof(char), read_size, fp);
				read_bytes += read_size;
				break;
			} else {	
				fseek(fp, current_id*block_size+open_file->position, SEEK_SET);
				const long sss = fread(buffer + read_bytes, sizeof(char), file_size, fp);
				read_bytes += file_size;
				break;
			}
		} else {
			if(read_size <= block_size){
				fseek(fp, current_id*block_size+open_file->position, SEEK_SET);
				const long sss = fread(buffer + read_bytes, sizeof(char), read_size, fp);
				read_bytes += read_size;
				break;
			} else {	
				if(read_bytes == 0){
					fseek(fp, current_id*block_size+open_file->position, SEEK_SET);
					const long sss = fread(buffer + read_bytes, sizeof(char), block_size - open_file->position, fp);
					read_bytes += block_size - open_file->position;
				} else {
					fseek(fp, current_id*block_size+open_file->position, SEEK_SET);
					const long sss = fread(buffer + read_bytes, sizeof(char), block_size, fp);
					read_bytes += block_size;
				}
			}
		}
	}
	
	open_file->position += read_bytes;
	
	char* etherhead = (char*)buffer;
	*(etherhead + size) = '\0';
	std::string string = std::string(etherhead);
	buffer = static_cast<void *>(etherhead); 
	fclose(fp);
	return read_bytes;
}


/**
 * Change the cursor position of an open file.
 * @param  offset     how much to change
 * @param  from_start whether to start from beginning of file (or current position)
 * @return            false if the new position is not available, true otherwise.
 */
bool mini_file_seek(FAT_FILESYSTEM *fs, FAT_OPEN_FILE * open_file, const int offset, const bool from_start)
{
	// TODO: seek and return true.
	FAT_FILE *file = open_file->file;
	int cp = open_file->position;
	if(from_start){
		if(offset > file->size || offset <0){
			return false;
		}
		open_file->position = offset;
	} else {
		if(open_file->position + offset > file->size || cp + offset < 0){
			return false;
		}
		open_file->position = cp + offset;
	}
	return true;
}

/**
 * Attemps to delete a fiif (!fd) {le from filesystem.
 * If the file is open, it cannot be deleted.
 * Marks the blocks of a deleted file as empty on the filesystem.
 * @return true on success, false on non-existing or open file.
 */
bool mini_file_delete(FAT_FILESYSTEM *fs, const char *filename)
{
	// TODO: delete file after checks.
	FAT_FILE * fd = mini_file_find(fs, filename);
	if (fd) {
		if((fd->open_handles).empty()){
			int metadata_index = fd->metadata_block_id;
			fs->block_map[metadata_index] = EMPTY_BLOCK;
			//TODO: consider the data
			std::vector<int> data_blocks = fd->block_ids;
			for(int i = 0; i < data_blocks.size(); i++){
				fs->block_map[i] = EMPTY_BLOCK;
			} 
			fd->block_ids.clear();
			fd->metadata_block_id = DELETED_METADATA;
			for(int j=0; j< fs->files.size(); j++){
				if(strcmp(fs->files[j]->name, filename) == 0){
					fs->files.erase(fs->files.begin() + j);
				}
			}

			return true;
		} else {
			return false;
		}
	}
	return false;
}
