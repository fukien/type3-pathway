#include "utils.h"

int nr_nodes = 0;
size_t file_idx = 0;
#ifdef USE_NVM
char nvm_local_dir[CHAR_BUFFER_LEN];
char nvm_remote_dir[CHAR_BUFFER_LEN];
#endif /* USE_NVM */
char disk_dir[CHAR_BUFFER_LEN];


void red() {
	printf("\033[1;31m");
	printf("\n");
}

void yellow() {
	printf("\033[1;33m");
	printf("\n");
}

void blue() {
	printf("\033[0;34m");
	printf("\n");
}

void green() {
	printf("\033[0;32m");
	printf("\n");
}

void cyan() {
	printf("\033[0;36m");
	printf("\n");
}

void purple() {
	printf("\033[0;35m");
	printf("\n");
}

void white() {
	printf("\033[0;37m");
	printf("\n");
}

void black() {
	printf("\033[0;30m");
	printf("\n");
}

void reset() {
	printf("\033[0m");
	printf("\n");
}

double diff_sec(const struct timespec start, const struct timespec end) {
	return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
}

void touchfile(const char * filename) {
	char command[CHAR_BUFFER_LEN] = {'\0'};
	strcat(strcpy(command, "touch "), filename);
	int val = system(command);
	if (val != 0) {
		printf("\"%s\" failed, errno: %d\n", command, errno);
		perror(command);
	}
}

void newdir(const char * dirpath) {
	char command[CHAR_BUFFER_LEN] = {'\0'};
	strcat(strcpy(command, "mkdir -p "), dirpath);
	int val = system(command);
	if (val != 0) {
		printf("\"%s\" failed, errno: %d\n", command, errno);
		perror(command);
	}
}

void deldir(const char * dirpath) {
	char command[CHAR_BUFFER_LEN] = {'\0'};
	strcat(strcpy(command, "rm -r "), dirpath);
	int val = system(command);
	if (val != 0) {
		printf("\"%s\" failed, errno: %d\n", command, errno);
		perror(command);
	}
}

void delfile(const char * filepath) {
	char command[CHAR_BUFFER_LEN] = {'\0'};
	strcat(strcpy(command, "rm "), filepath);
	int val = system(command);
	if (val != 0) {
		printf("\"%s\" failed, errno: %d\n", command, errno);
		perror(command);
	}
}

void renamefile(const char * oldname, const char * newname) {
	char command[CHAR_BUFFER_LEN] = {'\0'};
	strcat(strcat(strcat(strcpy(command, "mv "), oldname), " "), newname);
	int val = system(command);
	if (val != 0) {
		printf("\"%s\" failed, errno: %d\n", command, errno);
		perror(command);
	}
}

int file_exists(const char * filepath) {
	int val;
	int res = access(filepath, F_OK);
	if (res == 0) {
		val = 1;
	} else {
		val = 0;
		printf("%s access failed, errno: %d\n", filepath, errno);
		perror(filepath);
	}
	return val;
}

int dir_exists(const char * dirpath) {
	int val;
	DIR *dir = opendir(dirpath);
	if (dir) {
		val = 1;
		closedir(dir);
	} else {
		val = 0;
		printf("%s path not found, errno: %d\n", dirpath, errno);
		perror(dirpath);
	}
	return val;
}

void * create_pmem_buffer(const char *filepath, const size_t buffer_size) {
	void *pmemaddr;
	int is_pmem = 0;
	size_t mapped_len = 0;

	int res = access(filepath, F_OK);
	if (res == 0) {
		pmemaddr = (char*) pmem_map_file(filepath, buffer_size, 
			PMEM_FILE_CREATE, 0666, &mapped_len, &is_pmem);
	} else {
		// printf("create mapped filepath: %s\n", filepath);
		pmemaddr = (char*) pmem_map_file(filepath, buffer_size,
			PMEM_FILE_CREATE|PMEM_FILE_EXCL, 0666, &mapped_len, &is_pmem);
	}

	if (pmemaddr == NULL) {
		// printf("%s pmem_map_file failed, errno: %d\n", filepath, errno);
		// printf("arguments, buffer_size: %zu, mapped_len: %zu, is_pmem: %d\n", buffer_size, mapped_len, is_pmem);
		// perror("pmem_map_file failed");
		exit(1);
	}

	// if (is_pmem) {
	// 	// printf("successfully mapped\n");
	// } else {
	// 	printf("%s error, not pmem, is_pmem: %d", filepath, is_pmem);
	// 	exit(1);
	// }

	if (!is_pmem) {
		// printf("%s error, not pmem, is_pmem: %d", filepath, is_pmem);
		exit(1);
	}

	return pmemaddr;
}


void * alloc_dram(const size_t buffer_size, const int numa_node_idx) {
	struct bitmask *numa_nodes = numa_bitmask_alloc(nr_nodes);
	numa_bitmask_clearall(numa_nodes);
	numa_bitmask_setbit(numa_nodes, numa_node_idx);

	void *addr = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		perror("mmap");
		printf("buffer_size: %zu\n", buffer_size);
		exit (1);
	}
	int rv = mbind(addr, buffer_size, MPOL_BIND, numa_nodes->maskp, numa_nodes->size+1, 0);

	/**
		numa_bitmask_setbit(numa_nodes, 4);
		numa_bitmask_setbit(numa_nodes, 5);
		numa_bitmask_setbit(numa_nodes, 6);
		numa_bitmask_setbit(numa_nodes, 7);
		int rv = mbind(addr, buffer_size, MPOL_INTERLEAVE, numa_nodes->maskp, numa_nodes->size+1, 0);
	*/

	if (rv < 0) {
		perror("mbind");
		exit (1);
	}

#ifdef USE_HUGE
	rv = madvise(addr, buffer_size, MADV_HUGEPAGE);
#else /* USE_HUGE */
	rv = madvise(addr, buffer_size, MADV_NOHUGEPAGE);
#endif /* USE_HUGE */

	if (rv < 0) {
		perror("madvise");
		exit (1);
	}

	numa_bitmask_free(numa_nodes);
	return addr;
}


#ifdef USE_NVM
char * new_nvm_filepath(const char* const directory, const int numa_node_idx, const size_t thread_id) {
	char *buff = (char*) malloc( sizeof(char) * CHAR_BUFFER_LEN *2 );
	if (numa_node_idx == 1) {
		snprintf(buff, CHAR_BUFFER_LEN * 2, "%s/%lu_%zu.bin", nvm_local_dir, thread_id, file_idx ++);
	} else if (numa_node_idx == 0) {
		snprintf(buff, CHAR_BUFFER_LEN * 2, "%s/%lu_%zu.bin", nvm_remote_dir, thread_id, file_idx ++);
	} else {
		exit(1);
	}
	return buff;
}


void * new_alloc_nvm(const size_t buffer_size, const int numa_node_idx) {
	char file_path[ CHAR_BUFFER_LEN * 2 ];
	if (numa_node_idx == 1) {
		snprintf(file_path, CHAR_BUFFER_LEN * 2, "%s/%lu_%zu.bin", nvm_local_dir, pthread_self(), file_idx ++);
	} else if (numa_node_idx == 0) {
		snprintf(file_path, CHAR_BUFFER_LEN * 2, "%s/%lu_%zu.bin", nvm_remote_dir, pthread_self(), file_idx ++);
	} else {
		exit(1);
	}
	return create_pmem_buffer(file_path, buffer_size);
}
#endif /* USE_NVM */


void * alloc_memory(const size_t buffer_size, const int numa_node_idx) {
#ifdef USE_NVM
	char file_path[ CHAR_BUFFER_LEN * 2 ];
	if (numa_node_idx == 1) {
		snprintf(file_path, CHAR_BUFFER_LEN * 2, "%s/%lu_%zu.bin", nvm_local_dir, pthread_self(), file_idx ++);
	} else if (numa_node_idx == 0) {
		snprintf(file_path, CHAR_BUFFER_LEN * 2, "%s/%lu_%zu.bin", nvm_remote_dir, pthread_self(), file_idx ++);
	} else {
		exit(1);
	}
	return create_pmem_buffer(file_path, buffer_size);
#else /* USE_NVM */
	return alloc_dram(buffer_size, numa_node_idx);
#endif /* USE_NVM */
}


void dealloc_memory(void * addr, const size_t size) {
#ifdef USE_NVM
	pmem_unmap(addr, size);
#else /* USE_NVM */
	munmap(addr, size);
#endif /* USE_NVM */
}


void * map_disk_file(const char* const filepath, const size_t buffer_size) {
	int res = access(filepath, F_OK);
	int fd;

	if (res == 0) {
		fd = open(filepath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		if (fd == -1) {
			exit(errno);
		}

	} else {
		fd = open(filepath, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
		if (fd == -1) {
			exit(errno);
		}
		if (ftruncate(fd, buffer_size)) {
			exit(errno);
		}
 	}

	void *addr;
	addr = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if ( addr == MAP_FAILED) {
		exit(errno);
	}

	close(fd);


#ifdef USE_HUGE
	madvise(addr, buffer_size, MADV_HUGEPAGE);
#else /* USE_HUGE */
	madvise(addr, buffer_size, MADV_NOHUGEPAGE);
#endif /* USE_HUGE */

	return addr;
}

void memsync(const void * const mem_addr, const size_t size) {
#ifdef USE_NVM
	pmem_persist(mem_addr, size);
#else /* USE_NVM */
	pmem_msync(mem_addr, size);
#endif /* USE_NVM */
}


void * alloc_triv_dram(const size_t buffer_size) {
	void * addr = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#ifdef USE_HUGE
	madvise(addr, buffer_size, MADV_HUGEPAGE);
#else /* USE_HUGE */
	madvise(addr, buffer_size, MADV_NOHUGEPAGE);
#endif /* USE_NVM */
	return addr;
}



void knuth_shuffle(size_t* arr, size_t num_tuples) {
	for (size_t i = num_tuples - 1; i > 0; i--) {
		size_t j = RAND_RANGE(i);
		size_t tmp = arr[i];
		arr[i] = arr[j];
		arr[j] = tmp;
	}
}
































































































































