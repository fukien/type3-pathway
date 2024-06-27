#include <omp.h>

#include "inc/utils.h"



extern int nr_nodes;
extern size_t file_idx;
extern char nvm_local_dir[CHAR_BUFFER_LEN];

const size_t invalid_key = 0;

typedef struct {
	size_t key;
	size_t val;
} bucket_t;

typedef struct {
	double time_mgmt;
	double time_build;
	double time_probe;
	double time_total;
} timekeeper_t;


void random_unique_gen(size_t* arr, size_t num_tuples) {
	for (size_t i = 0; i < num_tuples; i++) {
		arr[i] = (i+1);
	}

	/* randomly shuffle elements */
	knuth_shuffle(arr, num_tuples);
}


void dummy_initialize(size_t* arr, size_t num_tuples) {
	for (size_t i = 0; i < num_tuples; i++) {
		arr[i] = i;
	}
}


static timekeeper_t join_test(
	size_t* h_fact_fkey, size_t* h_fact_val, 
	size_t* h_dim_key, size_t* h_dim_val, 
	size_t num_fact, size_t num_dim) {

#ifndef USE_INTERLEAVING
	int numa_node_idx = NUMA_NODE_IDX - 1;
#endif /* USE_INTERLEAVING */

	timekeeper_t time_taken = {0.0, 0.0, 0.0, 0.0};
	struct timespec begin, end;
	bucket_t* hash_table = NULL;\

#ifdef USE_NVM
	hash_table = (bucket_t* ) new_alloc_nvm(
		sizeof(bucket_t) * num_dim, numa_node_idx
	);
#else /* USE_NVM */
#ifdef 	USE_INTERLEAVING
	hash_table = (bucket_t* ) alloc_triv_dram(
		sizeof(bucket_t) * num_dim
	);
#else /* USE_INTERLEAVING */
	hash_table = (bucket_t* ) alloc_dram(
		sizeof(bucket_t) * num_dim, numa_node_idx
	);
#endif /* USE_INTERLEAVING */
#endif /* USE_NVM */

	clock_gettime(CLOCK_REALTIME, &begin);
	const __m512i zero = _mm512_setzero_si512(); // Set a 512-bit zero vector
	#pragma omp parallel for num_threads(THREAD_NUM) schedule(static, BATCH_SIZE)
	for (size_t I = 0; I < num_dim; I += 4) {
		_mm512_stream_si512((__m512i*)&hash_table[I], zero);
	}
	clock_gettime(CLOCK_REALTIME, &end);
	time_taken.time_mgmt = diff_sec(begin, end);


	clock_gettime(CLOCK_REALTIME, &begin);

	#pragma omp parallel for num_threads(THREAD_NUM) schedule(static, BATCH_SIZE)
	for (size_t I = 0; I < num_dim; I++) {
		size_t key = h_dim_key[I];
		size_t val = h_dim_val[I];
		size_t h = (key & (num_dim - 1));
		// #pragma omp critical
		// {
		// 	while (hash_table[h].key != invalid_key) {
		// 		h = (h + 1) & (num_dim - 1);
		// 	}
		// }
		hash_table[h].key = key;
		hash_table[h].val = val;
	}

	clock_gettime(CLOCK_REALTIME, &end);
	time_taken.time_build =  diff_sec(begin, end);


	clock_gettime(CLOCK_REALTIME, &begin);

	size_t full_checksum = 0;
	size_t full_match_cnt = 0;

	#pragma omp parallel num_threads(THREAD_NUM) reduction(+:full_checksum, full_match_cnt)
	{
		long long checksum = 0;
		long long match_cnt = 0;
		
		#pragma omp for schedule(static, BATCH_SIZE)
		for (size_t i = 0; i < num_fact; ++i) {
			size_t key = h_fact_fkey[i];
			size_t val = h_fact_val[i];
			size_t h = key & (num_dim - 1);

			if (key == hash_table[h].key) {
				checksum += val + hash_table[h].val;
				match_cnt += 1;
			} else {
				while (invalid_key != hash_table[h].key) {
					h = (h + 1) & (num_dim - 1);
					if (key == hash_table[h].key) {
						checksum += val + hash_table[h].val;
						match_cnt += 1;
						break;
					}
				}
			}
		}

		full_checksum += checksum;
		full_match_cnt += match_cnt;
	}

	clock_gettime(CLOCK_REALTIME, &end);
	time_taken.time_probe =  diff_sec(begin, end);

	clock_gettime(CLOCK_REALTIME, &begin);
#ifdef USE_NVM
	pmem_unmap(hash_table, sizeof(bucket_t) * num_dim);
#else /* USE_NVM */
	dealloc_memory(hash_table, sizeof(bucket_t) * num_dim);
#endif /* USE_NVM */
	clock_gettime(CLOCK_REALTIME, &end);
	time_taken.time_mgmt += diff_sec(begin, end);

	time_taken.time_total = time_taken.time_mgmt + \
		time_taken.time_build + time_taken.time_probe;

	// printf(
	// 	"full_match_cnt: %zu\tfull_checksum: %zu\n", 
	// 	full_match_cnt, full_checksum
	// );

	return time_taken;
}


int main(int argc, char **argv){
	srand(32768);

#ifndef USE_INTERLEAVING
	int numa_node_idx = NUMA_NODE_IDX - 1;
#endif /* USE_INTERLEAVING */

	nr_nodes = numa_num_configured_nodes();
	struct bitmask *numa_nodes = numa_bitmask_alloc(nr_nodes);

	size_t num_dim = 1UL << 28;
	size_t num_fact = 1UL << 30;
	size_t dim_allocated_size = (sizeof(size_t)*num_dim + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	size_t fact_allocated_size = (sizeof(size_t)*num_fact + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	size_t *h_fact_fkey = NULL;
	size_t *h_fact_val = NULL;
	size_t *h_dim_key = NULL;
	size_t *h_dim_val = NULL;

#ifdef USE_NVM
	snprintf(nvm_local_dir, CHAR_BUFFER_LEN, "%s/%s", NVM_LOCAL_PREFIX, current_timestamp());
	newdir(nvm_local_dir);
	h_fact_fkey = (size_t* ) new_alloc_nvm(fact_allocated_size, numa_node_idx);
	h_fact_val = (size_t* ) new_alloc_nvm(fact_allocated_size, numa_node_idx);
	h_dim_key = (size_t* ) new_alloc_nvm(dim_allocated_size, numa_node_idx);
	h_dim_val = (size_t* ) new_alloc_nvm(dim_allocated_size, numa_node_idx);
#else /* USE_NVM */
#ifdef 	USE_INTERLEAVING
	h_fact_fkey = (size_t* ) alloc_triv_dram(fact_allocated_size);
	h_fact_val = (size_t* ) alloc_triv_dram(fact_allocated_size);
	h_dim_key = (size_t* ) alloc_triv_dram(dim_allocated_size);
	h_dim_val = (size_t* ) alloc_triv_dram(dim_allocated_size);
#else /* USE_INTERLEAVING */
	h_fact_fkey = (size_t* ) alloc_dram(fact_allocated_size, numa_node_idx);
	h_fact_val = (size_t* ) alloc_dram(fact_allocated_size, numa_node_idx);
	h_dim_key = (size_t* ) alloc_dram(dim_allocated_size, numa_node_idx);
	h_dim_val = (size_t* ) alloc_dram(dim_allocated_size, numa_node_idx);
#endif /* USE_INTERLEAVING */
#endif /* USE_NVM */


	random_unique_gen(h_dim_key, num_dim);
	dummy_initialize(h_dim_val, num_dim);
	int iters = num_fact / num_dim;
	for (size_t i = 0; i < iters; i++) {
		size_t* tuples = h_fact_fkey + num_dim * i;
		random_unique_gen(tuples, num_dim);
	}
	size_t remainder = num_fact % num_dim;
	if (remainder > 0) {
		size_t* tuples = h_fact_fkey + num_dim * iters;
		random_unique_gen(tuples, remainder);
	}
	dummy_initialize(h_fact_val, num_fact);
	memsync(h_fact_fkey, fact_allocated_size);
	memsync(h_fact_val, fact_allocated_size);
	memsync(h_dim_key, dim_allocated_size);
	memsync(h_dim_val, dim_allocated_size);

	timekeeper_t total_time_taken = {0.0, 0.0, 0.0, 0.0};
	for (int idx=0; idx<RUN_NUM; idx++) {
		timekeeper_t time_taken = join_test(
			h_fact_fkey, h_fact_val, 
			h_dim_key, h_dim_val,
			num_fact, num_dim
		);

		printf(
			"\tjoin_test\ttime_mgmt: %.9f\ttime_build: %.9f\t"
			"time_probe: %.9f\ttime_total: %.9f\n",
			time_taken.time_mgmt, time_taken.time_build, 
			time_taken.time_probe, time_taken.time_total
		);

		if (idx > 0) {
			total_time_taken.time_mgmt += time_taken.time_mgmt;
			total_time_taken.time_build += time_taken.time_build;
			total_time_taken.time_probe += time_taken.time_probe;
			total_time_taken.time_total += time_taken.time_total;
		}
	}


	printf(
		"JOIN_TEST\tTIME_MGMT: %.9f\tTIME_BUILD: %.9f\t"
		"TIME_PROBE: %.9f\tTIME_TOTAL: %.9f\n",
		total_time_taken.time_mgmt/(RUN_NUM-1), 
		total_time_taken.time_build/(RUN_NUM-1), 
		total_time_taken.time_probe/(RUN_NUM-1), 
		total_time_taken.time_total/(RUN_NUM-1)
	);

#ifdef USE_NVM
	pmem_unmap(h_fact_fkey, fact_allocated_size);
	pmem_unmap(h_fact_val, fact_allocated_size);
	pmem_unmap(h_dim_key, dim_allocated_size);
	pmem_unmap(h_dim_val, dim_allocated_size);
	deldir(nvm_local_dir);
#else /* USE_NVM */
	dealloc_memory(h_fact_fkey, fact_allocated_size);
	dealloc_memory(h_fact_val, fact_allocated_size);
	dealloc_memory(h_dim_key, dim_allocated_size);
	dealloc_memory(h_dim_val, dim_allocated_size);
#endif /* USE_NVM */

	numa_bitmask_free(numa_nodes);

	return 0;
}



































































