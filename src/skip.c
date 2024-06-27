#include <omp.h>

#include "inc/utils.h"

extern int nr_nodes;
extern size_t file_idx;
extern char nvm_local_dir[CHAR_BUFFER_LEN];


static double skip_test (size_t* h_keys, size_t num_items, int num_skip) {
	struct timespec begin, end;
	clock_gettime(CLOCK_REALTIME, &begin);
	size_t counter = 0;

	#pragma omp parallel num_threads(THREAD_NUM)
	{
		size_t out = 0;
		#pragma omp for schedule(static, BATCH_SIZE) reduction(+:counter)
		for (size_t idx = 0; idx < num_items; idx += num_skip) {
			out += h_keys[idx];
		}
		counter += out;
	}

	clock_gettime(CLOCK_REALTIME, &end);
	return diff_sec(begin, end);
}

int main(int argc, char **argv){
	srand(32768);

#ifndef USE_INTERLEAVING
	int numa_node_idx = NUMA_NODE_IDX - 1;
#endif /* USE_INTERLEAVING */

	nr_nodes = numa_num_configured_nodes();
	struct bitmask *numa_nodes = numa_bitmask_alloc(nr_nodes);

	size_t num_items = 1UL << 31;
	size_t* h_in;

	size_t allocated_size = (sizeof(size_t)*num_items + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
#ifdef USE_NVM
	snprintf(nvm_local_dir, CHAR_BUFFER_LEN, "%s/%s", NVM_LOCAL_PREFIX, current_timestamp());
	newdir(nvm_local_dir);
	h_in = (size_t* ) new_alloc_nvm(
		allocated_size, 
		numa_node_idx
	);
#else /* USE_NVM */
#ifdef 	USE_INTERLEAVING
	h_in = (size_t* ) alloc_triv_dram(
		allocated_size
	);
#else /* USE_INTERLEAVING */
	h_in = (size_t* ) alloc_dram(
		allocated_size, 
		numa_node_idx
	);
#endif /* USE_INTERLEAVING */
#endif /* USE_NVM */

	#pragma omp parallel num_threads(THREAD_NUM)
	{
		unsigned int seed = omp_get_thread_num();
		#pragma omp parallel for schedule(static, BATCH_SIZE)
		for (size_t idx=0; idx<num_items; idx++) {
			h_in[idx] = (double) rand_r(&seed)/RAND_MAX;
		}
	}
	memsync(h_in, allocated_size);

	for (int num_skip = 1; num_skip <= 512; num_skip*=2) {
		double total_time_taken = 0.0;
		double total_bandwidth = 0.0;
		for (int idx = 0; idx < RUN_NUM; idx++) {
			double time_taken = skip_test(h_in, num_items, num_skip);
			double bandwidth = num_items * sizeof(size_t) / num_skip / time_taken;
			printf(
				"\tskip: %d\tbytes: %zu\ttime_taken: %.9f\tbandwidth: %.9f\n",
				num_skip, 
				num_items * sizeof(size_t) / num_skip, 
				time_taken, 
				bandwidth
			);
			if (idx > 0) {
				total_time_taken += time_taken;
				total_bandwidth += bandwidth;
			}
		}
		printf(
			"SKIP: %d\tBYTES: %zu\tTIME_TAKEN: %.9f\tBANDWIDTH: %.9f\n",
			num_skip,
			num_items * sizeof(size_t) / num_skip,
			total_time_taken/(RUN_NUM-1),
			total_bandwidth/(RUN_NUM-1)
		);
	}

#ifdef USE_NVM
	pmem_unmap(h_in, allocated_size);
	deldir(nvm_local_dir);
#else /* USE_NVM */
	dealloc_memory(h_in, allocated_size);
#endif /* USE_NVM */

	numa_bitmask_free(numa_nodes);

	return 0;
}




















































