#!/bin/sh
ROCKSDB_DIR=c_src/rocksdb
ROCKSDB_TAG=v5.21

if [ ! -d ${ROCKSDB_DIR} ]; then
  	# Control will enter here if rocksdb doesn't exist.
  	#git clone https://github.com/google/rocksdb.git
	(cd c_src && git clone https://github.com/pundunlabs/rocksdb.git && cd rocksdb && git checkout $ROCKSDB_TAG)
fi

if [ -L "${ROCKSDB_DIR}/librocksdb.so" ]
then
	echo "librocksdb.so found."
else
	echo "librocksdb.so not found."
	export INSTALL_PATH="."
	(cd ${ROCKSDB_DIR} && make shared_lib)
fi
