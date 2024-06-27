#include <omp.h>

#include "inc/utils.h"

extern int nr_nodes;
extern size_t file_idx;
extern char nvm_local_dir[CHAR_BUFFER_LEN];



#ifndef IDHASH
#define IDHASH(X, MASK, BITSKIP) (((X) & MASK) >> BITSKIP)
#endif /* IDHASH */


static double part_test(size_t* h_in, size_t* h_out, 
	size_t num_items, size_t num_part, size_t **h_hist) {
	struct timespec begin, end;
	clock_gettime(CLOCK_REALTIME, &begin);

	size_t batch_size = num_items / THREAD_NUM;

	#pragma omp parallel num_threads(THREAD_NUM)
	{
		int _tid = omp_get_thread_num();
		size_t tmp_hashkey = 0;
		size_t hashmask = num_part - 1;
		size_t accum_num = 0;
		size_t start_idx = batch_size * _tid;
		size_t end_idx = (_tid == THREAD_NUM-1) ? num_items : batch_size * (_tid+1);

		h_hist[_tid] = (size_t *)alloc_triv_dram(sizeof(size_t)*2*num_part);
		memset(h_hist[_tid], 0, sizeof(size_t)*2*num_part);
		size_t* my_hist = h_hist[_tid];
		size_t* h_output_offset = (size_t *) alloc_triv_dram(sizeof(size_t)*num_part);
		memset(h_output_offset, 0, sizeof(size_t)*num_part);

		#pragma omp barrier

		for (size_t idx = start_idx; idx < end_idx; idx ++) {
			tmp_hashkey = IDHASH(h_in[idx], hashmask, 0);
			my_hist[tmp_hashkey*2] ++;
		}

		for (size_t idx = 0; idx < num_part; idx ++) {
			accum_num += my_hist[idx*2];
			my_hist[idx*2+1] = accum_num;
		}

		#pragma omp barrier

		for (size_t idx = 0; idx < _tid; idx ++) {
			for (size_t jdx = 0; jdx < num_part; jdx ++) {
				h_output_offset[jdx] = h_hist[idx][jdx*2+1];
			}
		}

		for (size_t idx = _tid; idx < THREAD_NUM; idx ++) {
			for (size_t jdx = 1; jdx < num_part; jdx ++) {
				h_output_offset[jdx] = h_hist[idx][(jdx-1)*2+1];
			}
		}

		for (size_t idx = start_idx; idx < end_idx; idx ++) {
			tmp_hashkey = IDHASH(h_in[idx], hashmask, 0);
			h_out[h_output_offset[tmp_hashkey]] = h_in[idx];
			h_output_offset[tmp_hashkey] ++;
		}

		// for (size_t idx = 0; idx < num_part; idx ++) {
		// 	h_output_offset[idx] -= my_hist[idx * 2];
		// }

		dealloc_memory(h_output_offset, sizeof(size_t)*num_part);
		dealloc_memory(h_hist[_tid], sizeof(size_t)*2*num_part);
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

	size_t num_items = 1UL << 30;
	size_t* h_in;
	size_t* h_out;

	size_t allocated_size = (sizeof(size_t)*num_items + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
#ifdef USE_NVM
	snprintf(nvm_local_dir, CHAR_BUFFER_LEN, "%s/%s", NVM_LOCAL_PREFIX, current_timestamp());
	newdir(nvm_local_dir);
	h_in = (size_t* ) new_alloc_nvm(
		allocated_size, 
		numa_node_idx
	);
	h_out = (size_t *) new_alloc_nvm(
		allocated_size,
		numa_node_idx
	);
#else /* USE_NVM */
#ifdef 	USE_INTERLEAVING
	h_in = (size_t* ) alloc_triv_dram(
		allocated_size
	);
	h_out = (size_t* ) alloc_triv_dram(
		allocated_size
	);
#else /* USE_INTERLEAVING */
	h_in = (size_t* ) alloc_dram(
		allocated_size, 
		numa_node_idx
	);
	h_out = (size_t* ) alloc_dram(
		allocated_size, 
		numa_node_idx
	);
#endif /* USE_INTERLEAVING */
#endif /* USE_NVM */


	#pragma omp parallel num_threads(THREAD_NUM)
	{
		#pragma omp parallel for schedule(static, BATCH_SIZE)
		for (size_t idx=0; idx<num_items; idx++) {
			h_in[idx] = idx+1;
			h_out[idx] = 0;
		}
	}

	knuth_shuffle(h_in, num_items);
	memsync(h_in, allocated_size);
	memsync(h_out, allocated_size);
	size_t** h_hist = (size_t**) alloc_triv_dram( sizeof(size_t*) * THREAD_NUM );


	for (int num_part = 16; num_part <= 16384; num_part*=2) {
		double total_time_taken = 0.0;
		double total_bandwidth = 0.0;
		for (int idx = 0; idx < RUN_NUM; idx++) {
			double time_taken = part_test(h_in, h_out, num_items, num_part, h_hist);
			memset(h_out, 0, allocated_size);
			double bandwidth = num_items * sizeof(size_t) / time_taken;
			printf(
				"\tpart: %d\ttime_taken: %.9f\tbandwidth: %.9f\n",
				num_part, 
				time_taken, 
				bandwidth
			);
			if (idx > 0) {
				total_time_taken += time_taken;
				total_bandwidth += bandwidth;
			}
		}
		printf(
			"PART %d\tTIME_TAKEN: %.9f\tBANDWIDTH: %.9f\n",
			num_part,
			total_time_taken/(RUN_NUM-1),
			total_bandwidth/(RUN_NUM-1)
		);
	}
	dealloc_memory(h_hist, sizeof(size_t*) * THREAD_NUM);



#ifdef USE_NVM
	pmem_unmap(h_in, allocated_size);
	pmem_unmap(h_out, allocated_size);
	deldir(nvm_local_dir);
#else /* USE_NVM */
	dealloc_memory(h_in, allocated_size);
	dealloc_memory(h_out, allocated_size);
#endif /* USE_NVM */

	numa_bitmask_free(numa_nodes);

	return 0;
}




























































































































