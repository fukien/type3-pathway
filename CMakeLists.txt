cmake_minimum_required(VERSION 3.16.3)
project(type3-pathway-experiment)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS "-O3 -march=native -Wall")
# set(CMAKE_C_FLAGS "-O3 -march=native -Wall -ggdb3")
# set(CMAKE_C_FLAGS "-O0 -march=native -Wall -ggdb3")
# add_definitions(-DIN_DEBUG)
#ifdef IN_DEBUG
#endif /* IN_DEBUG */


# Find and configure OpenMP
find_package(OpenMP REQUIRED)

add_definitions(-DCHAR_BUFFER_LEN=256)


# We force NUMA_NODE_IDX start from 1, i.e., NUMA_NODE_IDX = numa_node_id + 1
if(NUMA_NODE_IDX)
	add_definitions(-DNUMA_NODE_IDX=${NUMA_NODE_IDX})
else()
	# add_definitions(-DNUMA_NODE_IDX=2)
	add_definitions(-DNUMA_NODE_IDX=8)
endif()

if(LOCAL_NODE_IDX)
	add_definitions(-DLOCAL_NODE_IDX=${LOCAL_NODE_IDX})
else()
	# add_definitions(-DLOCAL_NODE_IDX=2)
	add_definitions(-DLOCAL_NODE_IDX=8)
endif()

if(USE_CXL)
	add_definitions(-DUSE_CXL)
	add_definitions(-DCXL_NODE_IDX=9)
endif()

if(USE_NVM)
	add_definitions(-DUSE_NVM)
	add_definitions(-DNVM_LOCAL_PREFIX="/pmemfs1/huang/type3-pathway-experiment")
	add_definitions(-DNVM_REMOTE_PREFIX="/pmemfs0/huang/type3-pathway-experiment")
	add_definitions(-DNVM_LOCAL_SSB_PREFIX="${NVM_LOCAL_PREFIX}/ssb_data/s")
endif()

add_definitions(-DSSB_PREFIX="${CMAKE_SOURCE_DIR}/ssb_data/s")

if(SSB_BATCH_SIZE)
	add_definitions(-DSSB_BATCH_SIZE=${SSB_BATCH_SIZE})
else()
	add_definitions(-DSSB_BATCH_SIZE=32)
endif()

if(SF)
	add_definitions(-DSF=${SF})
else()
	add_definitions(-DSF=30)
endif()

if(USE_TIER)
	add_definitions(-DUSE_TIER)
endif()

if(USE_INTERLEAVING)
	add_definitions(-DUSE_INTERLEAVING)
endif()

if(THREAD_NUM)
	add_definitions(-DTHREAD_NUM=${THREAD_NUM})
else()
	add_definitions(-DTHREAD_NUM=16)
endif()

if(USE_HUGE)
	add_definitions(-DUSE_HUGE)
endif()

if(PAGE_SIZE)
	add_definitions(-DPAGE_SIZE=${PAGE_SIZE})
else()
	add_definitions(-DPAGE_SIZE=2097152)
endif()

if(ALIGNMENT)
	add_definitions(-DALIGNMENT=${ALIGNMENT})
else()
	add_definitions(-DALIGNMENT=64)
endif()

if(BATCH_SIZE)
	add_definitions(-DBATCH_SIZE=${BATCH_SIZE})
else()
	add_definitions(-DBATCH_SIZE=262144)
endif()

if(SELECTIVITY)
	add_definitions(-DSELECTIVITY=${SELECTIVITY})
else()
	add_definitions(-DSELECTIVITY=0.10)
endif()

if(RUN_NUM)
	add_definitions(-DRUN_NUM=${RUN_NUM})
else()
	add_definitions(-DRUN_NUM=6)
endif()



set(BIN_PATH "bin")
set(SRC_PATH "src")
set(INC_PATH "src/inc")

add_subdirectory(${INC_PATH})
link_libraries(Inc)

add_executable(bandwidth ${SRC_PATH}/bandwidth.c)
set_target_properties(bandwidth PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/${BIN_PATH}/)
target_link_libraries(bandwidth PUBLIC OpenMP::OpenMP_C)

add_executable(join ${SRC_PATH}/join.c)
set_target_properties(join PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/${BIN_PATH}/)
target_link_libraries(join PUBLIC OpenMP::OpenMP_C)

add_executable(skip ${SRC_PATH}/skip.c)
set_target_properties(skip PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/${BIN_PATH}/)
target_link_libraries(skip PUBLIC OpenMP::OpenMP_C)

add_executable(part ${SRC_PATH}/part.c)
set_target_properties(part PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/${BIN_PATH}/)
target_link_libraries(part PUBLIC OpenMP::OpenMP_C)

#[[
set(SSB_EXECUTABLES q11 q12 q13 q21 q22 q23 q31 q32 q33 q34 q41 q42 q43)
add_subdirectory(${SRC_PATH}/ssb/inc)
foreach(ssb_executable IN LISTS SSB_EXECUTABLES)
	set(source_file "${ssb_executable}.c")
	add_executable(${ssb_executable}  ${SRC_PATH}/ssb/${source_file})
	set_target_properties(${ssb_executable} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/${BIN_PATH}/ssb)
	target_link_libraries(${ssb_executable} OpenMP::OpenMP_C SSBInc)
endforeach()
]]














