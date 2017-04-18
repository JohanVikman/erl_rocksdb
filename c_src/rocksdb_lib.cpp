#include "rocksdb_nif.h"
#include <assert.h>

#include <iostream>

using namespace rocksdb;

namespace {
    DescendingComparator descendingcomparator;

    /* atoms */
    ERL_NIF_TERM atom_error;
    ERL_NIF_TERM atom_not_found;
    ERL_NIF_TERM atom_corruption;
    //ERL_NIF_TERM atom_io_error;
    ERL_NIF_TERM atom_invalid_argument;
    ERL_NIF_TERM atom_merge_in_progress;
    ERL_NIF_TERM atom_incomplete;
    ERL_NIF_TERM atom_shutdown_in_progress;
    ERL_NIF_TERM atom_timed_out;
    ERL_NIF_TERM atom_aborted;
    ERL_NIF_TERM atom_lock_limit;
    ERL_NIF_TERM atom_busy;
    ERL_NIF_TERM atom_deadlock;
    ERL_NIF_TERM atom_expired;
    ERL_NIF_TERM atom_try_again;
    ERL_NIF_TERM atom_no_space;

    int get_bool(ErlNifEnv* env, ERL_NIF_TERM term)
    {
	char buf[6];

	if(enif_get_atom(env, term, buf, sizeof(buf), ERL_NIF_LATIN1)){
	    if (strcmp("false", buf) == 0)
		return 0;
	    if (strcmp("true", buf) == 0)
		return 1;
	}
	return -1;
    }

    void delete_db_default(db_obj_resource* rdb){
	rocksdb::DB *db;
	db = (rocksdb::DB*) rdb->object;
	rdb->mtx->lock();
	delete rdb->link_set;
	delete db;
	rdb->allocated = 0;
	rdb->mtx->unlock();
    }

    void delete_db_with_ttl(db_obj_resource* rdb){
	rocksdb::DBWithTTL *db;
	db = (rocksdb::DBWithTTL*) rdb->object;
	rdb->mtx->lock();
	delete rdb->link_set;
	delete db;
	rdb->allocated = 0;
	rdb->mtx->unlock();
    }
}

void init_lib_atoms(ErlNifEnv* env) {
    atom_error = enif_make_atom(env, "error");
    atom_not_found = enif_make_atom(env, "not_found");
    atom_corruption = enif_make_atom(env, "corruption");
    atom_invalid_argument = enif_make_atom(env, "invalid_argument");
    atom_merge_in_progress = enif_make_atom(env, "merge_in_progress");
    atom_incomplete = enif_make_atom(env, "incomplete");
    atom_shutdown_in_progress = enif_make_atom(env, "shutdown_in_progress");
    atom_timed_out = enif_make_atom(env, "timed_out");
    atom_aborted = enif_make_atom(env, "aborted");
    atom_lock_limit = enif_make_atom(env, "lock_limit");
    atom_busy = enif_make_atom(env, "busy");
    atom_deadlock = enif_make_atom(env, "deadlock");
    atom_expired = enif_make_atom(env, "expired");
    atom_try_again = enif_make_atom(env, "try_again");
    atom_no_space = enif_make_atom(env, "no_space");
}

void delete_db(db_obj_resource* rdb){
    if(rdb->type == DB_WITH_TTL){
	delete_db_with_ttl( rdb );
    }
    else {
	delete_db_default ( rdb);
    }
}

void delete_rit(it_obj_resource* rit){
    db_obj_resource *rdb;
    unordered_set<void*> *set;

    rdb = (db_obj_resource*) rit->linked_obj;
    set = rdb->link_set;
    rdb->mtx->lock();
    set->erase(rit);
    rdb->mtx->unlock();

    rocksdb::Iterator *it;
    it = (rocksdb::Iterator*) rit->object;
    rit->mtx->lock();
    delete it;
    rit->allocated = 0;
    rit->mtx->unlock();
}

int fix_options(unordered_map<string, string>* map, rocksdb::Options* opts) {
    unordered_map<std::string,string>::const_iterator got = map->find ("comparator");
    if ( got != map->end() ){
	//found comparator option
	if( got->second.compare("descending") == 0) {
	    opts->comparator = &descendingcomparator;
	}
	map->erase("comparator");
    }
    return 0;
}

int init_readoptions(ErlNifEnv* env, const ERL_NIF_TERM* readoptions_array, rocksdb::ReadOptions **_readoptions) {
    int temp;
    rocksdb::ReadOptions *readoptions = new rocksdb::ReadOptions;
    *_readoptions = readoptions;

    /* Representation of erlang tuple:
    // 0. rocksdb_options,
    // 1. verify_checksums,
    // 2. fill_cache,
    // 3. snapshot,
    // 4. iterate_upper_bound,
    // 5. read_tier,
    // 6. tailing,
    // 7. managed
    */
    
    //Set verify_checksums
    temp = get_bool(env, readoptions_array[1]);
    if (temp == -1) {
	return -1;
    }
    else{
	readoptions->verify_checksums = temp;
    }
    
    //Set fill_cache
    temp = get_bool(env, readoptions_array[2]);
    if (temp == -1) {
	return -1;
    }
    else{
	readoptions->fill_cache = temp;
    }
    return 0;

    //Set tailing
    temp = get_bool(env, readoptions_array[6]);
    if (temp == -1) {
	return -1;
    }
    else{
	readoptions->tailing = temp;
    }
    return 0;

    //Set managed
    temp = get_bool(env, readoptions_array[7]);
    if (temp == -1) {
	return -1;
    }
    else{
	readoptions->managed = temp;
    }
    return 0;
}

int init_writeoptions(ErlNifEnv* env, const ERL_NIF_TERM* writeoptions_array, rocksdb::WriteOptions **_writeoptions) {
    int temp;
    rocksdb::WriteOptions *writeoptions = new rocksdb::WriteOptions;
    *_writeoptions = writeoptions;
    
    /* Representation of erlang tuple:
    // 0. rocksdb_writeoptions
    // 1. sync,
    // 2. disableWAL,
    // 3. ignore_missing_column_families,
    // 4. no_slowdown
    */
    
    //Set sync
    temp = get_bool(env, writeoptions_array[1]);
    if (temp == -1) {
	return -1;
    }
    else{
	writeoptions->sync = temp;
    }

    //Set disableWAL
    temp = get_bool(env, writeoptions_array[2]);
    if (temp == -1) {
	return -1;
    }
    else{
	writeoptions->disableWAL = temp;
    }

    //Set ignore_missing_column_families,
    temp = get_bool(env, writeoptions_array[3]);
    if (temp == -1) {
	return -1;
    }
    else{
	writeoptions->ignore_missing_column_families = temp;
    }

    //Set no_slowdown
    temp = get_bool(env, writeoptions_array[4]);
    if (temp == -1) {
	return -1;
    }
    else{
	writeoptions->no_slowdown = temp;
    }
    return 0;
}

rocksdb::DB* open_db(rocksdb::Options* options, char* path, rocksdb::Status* status) {
    rocksdb::DB* db;
    string path_string(path);
    *status = rocksdb::DB::Open(*options, path_string, &db);
    //Prints "Status: OK" if successful
    //cout << "Status: " << status->ToString() << "\n" << endl;
    //assert(status.ok());
    return db;
}

rocksdb::DBWithTTL* open_db_with_ttl(rocksdb::Options* options, char* path, int32_t* ttl, rocksdb::Status* status) {
    rocksdb::DBWithTTL* db;
    string path_string(path);
    bool read_only = false;
    *status = rocksdb::DBWithTTL::Open(*options, path_string, &db, *ttl, read_only);
    return db;
}

rocksdb::Status Get(db_obj_resource* rdb, rocksdb::ReadOptions* readoptions, rocksdb::Slice* key, string* value) {
    rocksdb::Status status;
    if (rdb->type == DB_WITH_TTL) {
	status = static_cast<rocksdb::DBWithTTL*>(rdb->object)->Get(*readoptions, *key, value);
	return status;
    } else {
	status = static_cast<rocksdb::DB*>(rdb->object)->Get(*readoptions, *key, value);
	return status;
    }
}

rocksdb::Status Put(db_obj_resource* rdb, rocksdb::WriteOptions* writeoptions, rocksdb::Slice* key, rocksdb::Slice* value){
    rocksdb::Status status;
    if (rdb->type == DB_WITH_TTL) {
	status = static_cast<rocksdb::DBWithTTL*>(rdb->object)->Put(*writeoptions, *key, *value);
	return status;
    } else {
	status = static_cast<rocksdb::DB*>(rdb->object)->Put(*writeoptions, *key, *value);
	return status;
    }
}

rocksdb::Status Delete(db_obj_resource* rdb, rocksdb::WriteOptions* writeoptions, rocksdb::Slice* key){
    rocksdb::Status status;
    if (rdb->type == DB_WITH_TTL) {
	status = static_cast<rocksdb::DBWithTTL*>(rdb->object)->Delete(*writeoptions, *key);
	return status;
    } else {
	status = static_cast<rocksdb::DB*>(rdb->object)->Delete(*writeoptions, *key);
	return status;
    }
}

rocksdb::Status Write(db_obj_resource* rdb, rocksdb::WriteOptions* writeoptions, rocksdb::WriteBatch* batch){
    rocksdb::Status status;
    if (rdb->type == DB_WITH_TTL) {
	status = static_cast<rocksdb::DBWithTTL*>(rdb->object)->Write(*writeoptions, batch);
	return status;
    } else {
	status = static_cast<rocksdb::DB*>(rdb->object)->Write(*writeoptions, batch);
	return status;
    }
}

void GetApproximateSizes(db_obj_resource* rdb, rocksdb::Range* ranges, unsigned int ranges_size, uint64_t* size){
    if (rdb->type == DB_WITH_TTL) {
	static_cast<rocksdb::DBWithTTL*>(rdb->object)->GetApproximateSizes(ranges, ranges_size, size);
    } else {
	static_cast<rocksdb::DB*>(rdb->object)->GetApproximateSizes(ranges, ranges_size, size);
    }
}

rocksdb::Iterator* NewIterator(db_obj_resource* rdb, rocksdb::ReadOptions* readoptions){
    if (rdb->type == DB_WITH_TTL) {
	rocksdb::Iterator* it = static_cast<rocksdb::DBWithTTL*>(rdb->object)->NewIterator(*readoptions);
	return it;
    } else {
	rocksdb::Iterator* it = static_cast<rocksdb::DB*>(rdb->object)->NewIterator(*readoptions);
	return it;
    }
}

ERL_NIF_TERM make_status_tuple(ErlNifEnv* env, rocksdb::Status* status){
    ERL_NIF_TERM type;
    if(status->IsNotFound()){
	type = atom_not_found;
    }
    else if(status->IsCorruption()){
	type = atom_corruption;
    }
    else if(status->IsIOError()){
	type = enif_make_string(env, status->ToString().c_str(), ERL_NIF_LATIN1);
	//type = atom_io_error;
    }
    else if(status->IsInvalidArgument()){
	type = atom_invalid_argument;
    }
    else if(status->IsMergeInProgress()){
	type = atom_merge_in_progress;
    }
    else if(status->IsIncomplete()){
	type = atom_incomplete;
    }
    else if(status->IsShutdownInProgress()){
	type = atom_shutdown_in_progress;
    }
    else if(status->IsTimedOut()){
	type = atom_timed_out;
    }
    else if(status->IsAborted()){
	type = atom_aborted;
    }
    else if(status->IsLockLimit()){
	type = atom_lock_limit;
    }
    else if(status->IsBusy()){
	type = atom_busy;
    }
    else if(status->IsDeadlock()){
	type = atom_deadlock;
    }
    else if(status->IsExpired()){
	type = atom_expired;
    }
    else if(status->IsTryAgain()){
	type = atom_try_again;
    }
    else if(status->IsNoSpace()){
	type = atom_no_space;
    }
    else {
	type = enif_make_string(env, status->ToString().c_str(), ERL_NIF_LATIN1);
    }
    //const char* stString = status->ToString().c_str();
    return enif_make_tuple2(env, atom_error, type);
}
