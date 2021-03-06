/* rocksdb_nif.h */
#ifndef ROCKSDB_NIF_H
#define ROCKSDB_NIF_H

#include <mutex>          // std::mutex
#include <vector>  //std::vector
#include <unordered_set>  //std::unordered_set

#include "rocksdb/db.h"
#include "rocksdb/utilities/db_ttl.h"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/comparator.h"
#include "rocksdb/write_batch.h"
#include "rocksdb/utilities/backupable_db.h"
#include "rocksdb/utilities/checkpoint.h"

#include "term_index_merger.h"
#include "term_prep.h"
#include "erl_nif.h"

#define MAXPATHLEN  255

#define DB_DEFAULT	0
#define DB_WITH_TTL	1

using namespace std;

typedef struct _opt_obj_resource {
  void *object;
} opt_obj_resource;

typedef struct _db_obj_resource {
  char allocated;
  unordered_set<void*>* link_set;
  mutex* mtx;
  void* object;
  ErlNifPid pid;
  int32_t ttl;
  char type;
  char db_open;
  rocksdb::ColumnFamilyOptions* cfd_options;
  rocksdb::ColumnFamilyOptions* cfi_options;
  rocksdb::ColumnFamilyOptions* cfr_options;
  vector<rocksdb::ColumnFamilyHandle*>* handles;
} db_obj_resource;

typedef struct _it_obj_resource {
  char allocated;
  mutex *mtx;
  void *linked_obj;
  void *object;
} it_obj_resource;

typedef struct _lru_obj_resource {
  void *object;
} lru_obj_resource;


extern void init_lib_atoms(ErlNifEnv* env);

extern void delete_db(db_obj_resource* rdb);

extern void delete_rit(it_obj_resource* rit);

extern void open_db(rocksdb::DBOptions* options,
		    char* path,
		    db_obj_resource* rdb,
		    rocksdb::Status* status);

extern int fix_cf_options(ErlNifEnv* env, ERL_NIF_TERM kvl,
			  db_obj_resource* rdb,
			  rocksdb::DBOptions* options,
			  rocksdb::Status &status,
			  int num_threads,
			  std::shared_ptr<rocksdb::Cache>* shared_lru);

extern int init_readoptions(ErlNifEnv* env,
			    const ERL_NIF_TERM* readoptions_array,
			    rocksdb::ReadOptions **readoptions);

extern int init_writeoptions(ErlNifEnv* env,
			     const ERL_NIF_TERM* writeoptions_array,
			     rocksdb::WriteOptions **writeoptions);

extern void init_db(rocksdb::DB* db);

extern rocksdb::Status Get(db_obj_resource* rdb,
			   rocksdb::ReadOptions* readoptions,
			   int cf,
			   rocksdb::Slice* key,
			   rocksdb::PinnableSlice* value);

extern rocksdb::Status Put(db_obj_resource* rdb,
			   rocksdb::WriteOptions* writeoptions,
			   rocksdb::Slice* key,
			   rocksdb::Slice* value);

extern rocksdb::Status PutTerms(db_obj_resource* rdb,
				rocksdb::WriteOptions* writeoptions,
				rocksdb::Slice* key,
				rocksdb::Slice* value,
				std::vector<std::pair<Term, std::vector<Term>>> indices);

extern rocksdb::Status Delete(db_obj_resource* rdb,
			      rocksdb::WriteOptions* writeoptions,
			      rocksdb::Slice* key);

extern rocksdb::Status DeleteTerms(db_obj_resource* rdb,
				   rocksdb::WriteOptions* writeoptions,
				   rocksdb::Slice* key,
				   std::vector<Term> cids);

extern rocksdb::Status Write(db_obj_resource* rdb,
			     rocksdb::WriteOptions* writeoptions,
			     rocksdb::WriteBatch* batch);

extern void GetApproximateSizes(db_obj_resource* rdb,
				rocksdb::Range* ranges,
				unsigned int ranges_size,
				uint64_t* size);

extern rocksdb::Iterator* NewIterator(db_obj_resource* rdb,
				      rocksdb::ReadOptions* readoptions);

extern void CompactDB(db_obj_resource* rdb);

extern void CompactIndex(db_obj_resource* rdb);

extern rocksdb::Status BackupDB(db_obj_resource* rdb,
				char* path);

extern rocksdb::Status RestoreDB(char* bkp_path,
				 char* db_path,
				 char* wal_path);

extern rocksdb::Status RestoreDB(char* bkp_path,
				 char* db_path,
				 char* wal_path,
				 uint32_t backup_id);

extern rocksdb::Status CreateCheckpoint(db_obj_resource* rdb,
					char* path);

extern ERL_NIF_TERM make_status_tuple(ErlNifEnv* env,
				      rocksdb::Status* status);
extern int parse_int_pairs(ErlNifEnv* env,
			    ERL_NIF_TERM add_list,
			    vector< pair<int,int> >* list);

void SetTtl(db_obj_resource *rdb, int32_t ttl);

extern ERL_NIF_TERM rocksdb_memory_usage(ErlNifEnv* env, db_obj_resource* rdb);

#endif /*ROCKSDB_NIF_H*/
