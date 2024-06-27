date
start_time=$(date +%s)


CUR_DIR=$(pwd)
DIR_PATH=${CUR_DIR}/../../logs/microbenchmark-logs
mkdir -p $DIR_PATH

_3_LOG=${DIR_PATH}/join_3.log
rm $_3_LOG
_7_LOG=${DIR_PATH}/join_7.log
rm $_7_LOG
_8_LOG=${DIR_PATH}/join_8.log
rm $_8_LOG
_32_LOG=${DIR_PATH}/join_32.log
rm $_32_LOG


sudo sysctl -w vm.numa_tier_interleave="3 2"


RUN_NUM=6


for idx in `seq 32 63`; do
	THREAD_NUM=$((idx - 32 + 1))

	cd ../..
	bash clean.sh
	mkdir build
	cd build
	cmake .. -DUSE_HUGE=true -DUSE_CXL=true -DTHREAD_NUM=$THREAD_NUM -DRUN_NUM=$RUN_NUM -DNUMA_NODE_IDX=4
	make -j 16
	cd ..
	numactl --physcpubind=32-$idx ./bin/join >> $_3_LOG 2>&1
	cd ${CUR_DIR}

	cd ../..
	bash clean.sh
	mkdir build
	cd build
	cmake .. -DUSE_HUGE=true -DUSE_CXL=true -DTHREAD_NUM=$THREAD_NUM -DRUN_NUM=$RUN_NUM -DNUMA_NODE_IDX=8
	make -j 16
	cd ..
	numactl --physcpubind=32-$idx ./bin/join >> $_7_LOG 2>&1
	cd ${CUR_DIR}

	cd ../..
	bash clean.sh
	mkdir build
	cd build
	cmake .. -DUSE_HUGE=true -DUSE_CXL=true -DTHREAD_NUM=$THREAD_NUM -DRUN_NUM=$RUN_NUM -DNUMA_NODE_IDX=9
	make -j 16
	cd ..
	numactl --physcpubind=32-$idx ./bin/join >> $_8_LOG 2>&1
	cd ${CUR_DIR}

	cd ../..
	bash clean.sh
	mkdir build
	cd build
	cmake .. -DUSE_HUGE=true -DUSE_CXL=true -DTHREAD_NUM=$THREAD_NUM -DRUN_NUM=$RUN_NUM -DNUMA_NODE_IDX=8 -DUSE_INTERLEAVING=true
	make -j 16
	cd ..
	numactl --physcpubind=32-$idx --interleave=7,8 ./bin/join >> $_32_LOG 2>&1
	cd ${CUR_DIR}
done


cd ../..
bash clean.sh

sudo sysctl -w vm.numa_tier_interleave="1 1"


end_time=$(date +%s)
duration=$((end_time - start_time))
echo "Duration: $duration seconds"
date