link_libraries(-lm -lpmem -lnuma)
aux_source_directory(. DIR_LIB_SRCS)
add_library (Inc SHARED ${DIR_LIB_SRCS})