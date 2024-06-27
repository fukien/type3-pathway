# clear pagecache only.
sync; echo 1 > /proc/sys/vm/drop_caches

# clear dentries and inodes.
sync; echo 2 > /proc/sys/vm/drop_caches

# clear pagecache, dentries and inodes.
sync; echo 3 > /proc/sys/vm/drop_caches