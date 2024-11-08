date
start_time=$(date +%s)


CUR_DIR=$(pwd)
DIR_PATH=${CUR_DIR}/../../logs/ssb-logs
mkdir -p $DIR_PATH


_7_LOG=${DIR_PATH}/ssb_7.log
rm $_7_LOG
_8_LOG=${DIR_PATH}/ssb_8.log
rm $_8_LOG
_8_TIER_LOG=${DIR_PATH}/ssb_tier_8.log
rm $_8_TIER_LOG
_32_LOG=${DIR_PATH}/ssb_32.log
rm $_32_LOG


sudo sysctl -w vm.numa_tier_interleave="3 2"


RUN_NUM=6
THREAD_NUM=32


cd ../..
bash clean.sh
mkdir build
cd build
cmake .. -DUSE_HUGE=true -DUSE_CXL=true -DTHREAD_NUM=$THREAD_NUM -DRUN_NUM=$RUN_NUM -DSF=30 -DNUMA_NODE_IDX=8
make -j 16
cd ..
for query in q11 q12 q13 q21 q22 q23 q31 q32 q33 q34 q41 q42 q43; do
	numactl --physcpubind=32-63 ./bin/ssb/$query >> $_7_LOG 2>&1
done
cd ${CUR_DIR}

cd ../..
bash clean.sh
mkdir build
cd build
cmake .. -DUSE_HUGE=true -DUSE_CXL=true -DTHREAD_NUM=$THREAD_NUM -DRUN_NUM=$RUN_NUM -DSF=30 -DNUMA_NODE_IDX=9
make -j 16
cd ..
for query in q11 q12 q13 q21 q22 q23 q31 q32 q33 q34 q41 q42 q43; do
	numactl --physcpubind=32-63 ./bin/ssb/$query >> $_8_LOG 2>&1
done
cd ${CUR_DIR}




cd ../..
bash clean.sh
mkdir build
cd build
cmake .. -DUSE_HUGE=true -DUSE_CXL=true -DTHREAD_NUM=$THREAD_NUM -DRUN_NUM=$RUN_NUM -DSF=30 -DNUMA_NODE_IDX=9 -DUSE_TIER=true
make -j 16
cd ..
for query in q11 q12 q13 q21 q22 q23 q31 q32 q33 q34 q41 q42 q43; do
	numactl --physcpubind=32-63 ./bin/ssb/$query >> $_8_TIER_LOG 2>&1
done
cd ${CUR_DIR}


cd ../..
bash clean.sh
mkdir build
cd build
cmake .. -DUSE_HUGE=true -DUSE_CXL=true -DTHREAD_NUM=$THREAD_NUM -DRUN_NUM=$RUN_NUM -DNUMA_NODE_IDX=8 -DUSE_INTERLEAVING=true -DSF=30
make -j 16
cd ..
for query in q11 q12 q13 q21 q22 q23 q31 q32 q33 q34 q41 q42 q43; do
	numactl --physcpubind=32-63 --interleave=7,8 ./bin/ssb/$query >> $_32_LOG 2>&1
done
cd ${CUR_DIR}



cd ../..
bash clean.sh

sudo sysctl -w vm.numa_tier_interleave="1 1"



end_time=$(date +%s)
duration=$((end_time - start_time))
echo "Duration: $duration seconds"
date