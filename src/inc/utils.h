#ifndef UTILS_H
#define UTILS_H


#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <immintrin.h>
#include <libpmem.h>
#include <numa.h>
#include <numaif.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <xmmintrin.h>
#include <unistd.h>


#define RAND_RANGE(N) ((double)rand() / ((double)RAND_MAX + 1) * (N))


void red();
void yellow();
void blue();
void green();
void cyan();
void purple();
void white();
void black();
void reset();
double diff_sec(const struct timespec start, const struct timespec end);
void touchfile(const char * filename);
void newdir(const char * dirpath);
void deldir(const char * dirpath);
void delfile(const char * filepath);
void renamefile(const char * oldname, const char * newname);
int file_exists(const char * filepath);
int dir_exists(const char * dirpath);
void * create_pmem_buffer(const char *filepath, const size_t buffer_size);
void * alloc_dram(const size_t buffer_size, const int numa_node_idx);
#ifdef USE_NVM
char * new_nvm_filepath(const char* const directory, const int numa_node_idx, const size_t thread_id);
void * new_alloc_nvm(const size_t buffer_size, const int numa_node_idx);
#endif /* USE_NVM */
void * alloc_memory(const size_t buffer_size, const int numa_node_idx);
void dealloc_memory(void * addr, const size_t size);
void * map_disk_file(const char* const filepath, const size_t buffer_size);
void memsync(const void * const mem_addr, const size_t size);
void * alloc_triv_dram(const size_t buffer_size);
void knuth_shuffle(size_t* arr, size_t num_tuples);

#endif /* UTILS_H */
















































































