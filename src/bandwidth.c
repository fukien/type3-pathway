#include <omp.h>

#include "inc/utils.h"

extern int nr_nodes;
extern size_t file_idx;
extern char nvm_local_dir[CHAR_BUFFER_LEN];


static double read_test(size_t* h_keys, size_t num_items) {
	double result = 0.0;
	struct timespec begin, end;
	clock_gettime(CLOCK_REALTIME, &begin);
	#pragma omp parallel for num_threads(THREAD_NUM) reduction(+:result) schedule(static, BATCH_SIZE)
	for (size_t idx = 0; idx < num_items; idx++) {
		result += h_keys[idx];
	}
	clock_gettime(CLOCK_REALTIME, &end);
	// printf("read result: %f\n", result);
	return diff_sec(begin, end);
}


static double write_test(size_t* h_keys, size_t num_items) {
	struct timespec begin, end;
	clock_gettime(CLOCK_REALTIME, &begin);
	#pragma omp parallel for num_threads(THREAD_NUM) schedule(static, BATCH_SIZE)
	for (size_t idx = 0; idx < num_items; ++idx) {
		h_keys[idx] = idx;
	}
	clock_gettime(CLOCK_REALTIME, &end);
	return diff_sec(begin, end);
}


static double nt_read_test(size_t* h_keys, size_t num_items) {
	__m512i global_counter = _mm512_set1_epi64(0);
	struct timespec begin, end;
	clock_gettime(CLOCK_REALTIME, &begin);
	#pragma omp parallel num_threads(THREAD_NUM)
	{
		__m512i local_counter = _mm512_set1_epi64(0);

		#pragma omp for schedule(static, BATCH_SIZE)
		for (size_t idx = 0; idx < num_items; idx += 8) {
			__m512i load = _mm512_load_si512(&h_keys[idx]);
			local_counter = _mm512_add_epi64(load, local_counter);
		}

		#pragma omp critical
		{
			global_counter = _mm512_add_epi64(local_counter, global_counter);
		}
	}
	clock_gettime(CLOCK_REALTIME, &end);
	// size_t sum[8];
	// _mm512_store_epi64(sum, global_counter);
	// size_t total_sum = sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5] + sum[6] + sum[7];
	return diff_sec(begin, end);
}


static double nt_write_test(size_t* h_keys, size_t num_items) {
	struct timespec begin, end;
	clock_gettime(CLOCK_REALTIME, &begin);
	#pragma omp parallel for num_threads(THREAD_NUM) schedule(static, BATCH_SIZE)
	for (size_t idx = 0; idx < num_items; idx += 8) {
		__m512i ones = _mm512_set1_epi64(1);
		_mm512_stream_si512((__m512i*) &h_keys[idx], ones);
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
	size_t* h_keys;
	size_t allocated_size = (sizeof(size_t)*num_items + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

#ifdef USE_NVM
	snprintf(nvm_local_dir, CHAR_BUFFER_LEN, "%s/%s", NVM_LOCAL_PREFIX, current_timestamp());
	newdir(nvm_local_dir);
	h_keys = (size_t* ) new_alloc_nvm(allocated_size, numa_node_idx);
#else /* USE_NVM */
#ifdef 	USE_INTERLEAVING
	h_keys = (size_t* ) alloc_triv_dram(allocated_size);
#else /* USE_INTERLEAVING */
	h_keys = (size_t* ) alloc_dram(allocated_size, numa_node_idx);
#endif /* USE_INTERLEAVING */
#endif /* USE_NVM */

	#pragma omp parallel num_threads(THREAD_NUM)
	{
		unsigned int seed = omp_get_thread_num();
		#pragma omp for schedule(static, 32 * 1024)
		for (size_t idx = 0; idx < num_items; idx++) {
			h_keys[idx] = rand_r(&seed) & 15;
		}
	}

	memsync(h_keys, allocated_size);

	double total_time_taken = 0.0;
	double total_bandwidth = 0.0;
	for (int idx=0; idx<RUN_NUM; idx++) {
		double time_taken = read_test(h_keys, num_items);
		double bandwidth = num_items * sizeof(size_t) / time_taken;
		printf(
			"\tread\ttime_taken: %.9f\tbandwidth: %.9f\n",
			time_taken, bandwidth
		);
		if (idx > 0) {
			total_time_taken += time_taken;
			total_bandwidth += bandwidth;
		}
	}
	printf(
		"READ\tTIME_TAKEN: %.9f\tBANDWIDTH: %.9f\n",
		total_time_taken/(RUN_NUM-1),
		total_bandwidth/(RUN_NUM-1)
	);


	total_time_taken = 0.0;
	total_bandwidth = 0.0;
	for (int idx=0; idx<RUN_NUM; idx++) {
		double time_taken = write_test(h_keys, num_items);
		double bandwidth = num_items * sizeof(size_t) / time_taken;
		printf(
			"\twrite\ttime_taken: %.9f\tbandwidth: %.9f\n",
			time_taken, bandwidth
		);
		if (idx > 0) {
			total_time_taken += time_taken;
			total_bandwidth += bandwidth;
		}
	}
	printf(
		"WRITE\tTIME_TAKEN: %.9f\tBANDWIDTH: %.9f\n",
		total_time_taken/(RUN_NUM-1),
		total_bandwidth/(RUN_NUM-1)
	);


	total_time_taken = 0.0;
	total_bandwidth = 0.0;
	for (int idx=0; idx<RUN_NUM; idx++) {
		double time_taken = nt_read_test(h_keys, num_items);
		double bandwidth = num_items * sizeof(size_t) / time_taken;
		printf(
			"\tnt_read\ttime_taken: %.9f\tbandwidth: %.9f\n",
			time_taken, bandwidth
		);
		if (idx > 0) {
			total_time_taken += time_taken;
			total_bandwidth += bandwidth;
		}
	}
	printf(
		"NT_READ\tTIME_TAKEN: %.9f\tBANDWIDTH: %.9f\n",
		total_time_taken/(RUN_NUM-1),
		total_bandwidth/(RUN_NUM-1)
	);


	total_time_taken = 0.0;
	total_bandwidth = 0.0;
	for (int idx=0; idx<RUN_NUM; idx++) {
		double time_taken = nt_write_test(h_keys, num_items);
		double bandwidth = num_items * sizeof(size_t) / time_taken;
		printf(
			"\tnt_write\ttime_taken: %.9f\tbandwidth: %.9f\n",
			time_taken, bandwidth
		);
		if (idx > 0) {
			total_time_taken += time_taken;
			total_bandwidth += bandwidth;
		}
	}
	printf(
		"NT_WRITE\tTIME_TAKEN: %.9f\tBANDWIDTH: %.9f\n",
		total_time_taken/(RUN_NUM-1),
		total_bandwidth/(RUN_NUM-1)
	);

#ifdef USE_NVM
	pmem_unmap(h_keys, allocated_size);
	deldir(nvm_local_dir);
#else /* USE_NVM */
	dealloc_memory(h_keys, allocated_size);
#endif /* USE_NVM */

	numa_bitmask_free(numa_nodes);

	return 0;
}






























































































