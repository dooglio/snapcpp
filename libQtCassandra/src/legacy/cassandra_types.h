/**
 * These are legacy types originally generated by the thrift compiler.
 * They are intended to provide an interface to transition from thrift
 * to CQL use via the cassandra-cpp interface.
 */
#pragma once

#pragma GCC system_header

// --- Snap! Website header additions ---
#include <inttypes.h>


namespace org { namespace apache { namespace cassandra {


struct IndexType
{
  enum type
  {
    KEYS = 0,
    CUSTOM = 1,
    COMPOSITES = 2
  };
};

//extern const std::map<int, const char*> _IndexType_VALUES_TO_NAMES;


typedef struct _ColumnDef__isset
{
  _ColumnDef__isset() : index_type(false), index_name(false), index_options(false) {}
  bool index_type;
  bool index_name;
  bool index_options;
}
_ColumnDef__isset;

class ColumnDef
{
 public:

  static const char* ascii_fingerprint; // = "0D89CE83D7EDAD079AC3213ED1DCAA58";
  static const uint8_t binary_fingerprint[16]; // = {0x0D,0x89,0xCE,0x83,0xD7,0xED,0xAD,0x07,0x9A,0xC3,0x21,0x3E,0xD1,0xDC,0xAA,0x58};

  ColumnDef() : name(), validation_class(), index_type((IndexType::type)0), index_name() {
  }

  virtual ~ColumnDef() throw() {}

  std::string name;
  std::string validation_class;
  IndexType::type index_type;
  std::string index_name;
  std::map<std::string, std::string>  index_options;

  _ColumnDef__isset __isset;

  void __set_name(const std::string& val) {
    name = val;
  }

  void __set_validation_class(const std::string& val) {
    validation_class = val;
  }

  void __set_index_type(const IndexType::type val) {
    index_type = val;
    __isset.index_type = true;
  }

  void __set_index_name(const std::string& val) {
    index_name = val;
    __isset.index_name = true;
  }

  void __set_index_options(const std::map<std::string, std::string> & val) {
    index_options = val;
    __isset.index_options = true;
  }

  bool operator == (const ColumnDef & rhs) const
  {
    if (!(name == rhs.name))
      return false;
    if (!(validation_class == rhs.validation_class))
      return false;
    if (__isset.index_type != rhs.__isset.index_type)
      return false;
    else if (__isset.index_type && !(index_type == rhs.index_type))
      return false;
    if (__isset.index_name != rhs.__isset.index_name)
      return false;
    else if (__isset.index_name && !(index_name == rhs.index_name))
      return false;
    if (__isset.index_options != rhs.__isset.index_options)
      return false;
    else if (__isset.index_options && !(index_options == rhs.index_options))
      return false;
    return true;
  }
  bool operator != (const ColumnDef &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ColumnDef & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _CfDef__isset
{
  _CfDef__isset() : column_type(true), comparator_type(true), subcomparator_type(false), comment(false), read_repair_chance(false), column_metadata(false), gc_grace_seconds(false), default_validation_class(false), id(false), min_compaction_threshold(false), max_compaction_threshold(false), replicate_on_write(false), key_validation_class(false), key_alias(false), compaction_strategy(false), compaction_strategy_options(false), compression_options(false), bloom_filter_fp_chance(false), caching(true), dclocal_read_repair_chance(true), populate_io_cache_on_flush(false), memtable_flush_period_in_ms(false), default_time_to_live(false), index_interval(false), speculative_retry(true), triggers(false), row_cache_size(false), key_cache_size(false), row_cache_save_period_in_seconds(false), key_cache_save_period_in_seconds(false), memtable_flush_after_mins(false), memtable_throughput_in_mb(false), memtable_operations_in_millions(false), merge_shards_chance(false), row_cache_provider(false), row_cache_keys_to_save(false) {}
  bool column_type;
  bool comparator_type;
  bool subcomparator_type;
  bool comment;
  bool read_repair_chance;
  bool column_metadata;
  bool gc_grace_seconds;
  bool default_validation_class;
  bool id;
  bool min_compaction_threshold;
  bool max_compaction_threshold;
  bool replicate_on_write;
  bool key_validation_class;
  bool key_alias;
  bool compaction_strategy;
  bool compaction_strategy_options;
  bool compression_options;
  bool bloom_filter_fp_chance;
  bool caching;
  bool dclocal_read_repair_chance;
  bool populate_io_cache_on_flush;
  bool memtable_flush_period_in_ms;
  bool default_time_to_live;
  bool index_interval;
  bool speculative_retry;
  bool triggers;
  bool row_cache_size;
  bool key_cache_size;
  bool row_cache_save_period_in_seconds;
  bool key_cache_save_period_in_seconds;
  bool memtable_flush_after_mins;
  bool memtable_throughput_in_mb;
  bool memtable_operations_in_millions;
  bool merge_shards_chance;
  bool row_cache_provider;
  bool row_cache_keys_to_save;
}
_CfDef__isset;

class CfDef
{
 public:

  static const char* ascii_fingerprint; // = "3D2F73880CC64DDE3E1F25480C58FD72";
  static const uint8_t binary_fingerprint[16]; // = {0x3D,0x2F,0x73,0x88,0x0C,0xC6,0x4D,0xDE,0x3E,0x1F,0x25,0x48,0x0C,0x58,0xFD,0x72};

  CfDef() : keyspace(), name(), column_type("Standard"), comparator_type("BytesType"), subcomparator_type(), comment(), read_repair_chance(0), gc_grace_seconds(0), default_validation_class(), id(0), min_compaction_threshold(0), max_compaction_threshold(0), replicate_on_write(0), key_validation_class(), key_alias(), compaction_strategy(), bloom_filter_fp_chance(0), caching("keys_only"), dclocal_read_repair_chance(0), populate_io_cache_on_flush(0), memtable_flush_period_in_ms(0), default_time_to_live(0), index_interval(0), speculative_retry("NONE"), row_cache_size(0), key_cache_size(0), row_cache_save_period_in_seconds(0), key_cache_save_period_in_seconds(0), memtable_flush_after_mins(0), memtable_throughput_in_mb(0), memtable_operations_in_millions(0), merge_shards_chance(0), row_cache_provider(), row_cache_keys_to_save(0) {
  }

  virtual ~CfDef() throw() {}

  std::string keyspace;
  std::string name;
  std::string column_type;
  std::string comparator_type;
  std::string subcomparator_type;
  std::string comment;
  double read_repair_chance;
  std::vector<ColumnDef>  column_metadata;
  int32_t gc_grace_seconds;
  std::string default_validation_class;
  int32_t id;
  int32_t min_compaction_threshold;
  int32_t max_compaction_threshold;
  bool replicate_on_write;
  std::string key_validation_class;
  std::string key_alias;
  std::string compaction_strategy;
  std::map<std::string, std::string>  compaction_strategy_options;
  std::map<std::string, std::string>  compression_options;
  double bloom_filter_fp_chance;
  std::string caching;
  double dclocal_read_repair_chance;
  bool populate_io_cache_on_flush;
  int32_t memtable_flush_period_in_ms;
  int32_t default_time_to_live;
  int32_t index_interval;
  std::string speculative_retry;
  std::vector<TriggerDef>  triggers;
  double row_cache_size;
  double key_cache_size;
  int32_t row_cache_save_period_in_seconds;
  int32_t key_cache_save_period_in_seconds;
  int32_t memtable_flush_after_mins;
  int32_t memtable_throughput_in_mb;
  double memtable_operations_in_millions;
  double merge_shards_chance;
  std::string row_cache_provider;
  int32_t row_cache_keys_to_save;

  _CfDef__isset __isset;

  void __set_keyspace(const std::string& val) {
    keyspace = val;
  }

  void __set_name(const std::string& val) {
    name = val;
  }

  void __set_column_type(const std::string& val) {
    column_type = val;
    __isset.column_type = true;
  }

  void __set_comparator_type(const std::string& val) {
    comparator_type = val;
    __isset.comparator_type = true;
  }

  void __set_subcomparator_type(const std::string& val) {
    subcomparator_type = val;
    __isset.subcomparator_type = true;
  }

  void __set_comment(const std::string& val) {
    comment = val;
    __isset.comment = true;
  }

  void __set_read_repair_chance(const double val) {
    read_repair_chance = val;
    __isset.read_repair_chance = true;
  }

  void __set_column_metadata(const std::vector<ColumnDef> & val) {
    column_metadata = val;
    __isset.column_metadata = true;
  }

  void __set_gc_grace_seconds(const int32_t val) {
    gc_grace_seconds = val;
    __isset.gc_grace_seconds = true;
  }

  void __set_default_validation_class(const std::string& val) {
    default_validation_class = val;
    __isset.default_validation_class = true;
  }

  void __set_id(const int32_t val) {
    id = val;
    __isset.id = true;
  }

  void __set_min_compaction_threshold(const int32_t val) {
    min_compaction_threshold = val;
    __isset.min_compaction_threshold = true;
  }

  void __set_max_compaction_threshold(const int32_t val) {
    max_compaction_threshold = val;
    __isset.max_compaction_threshold = true;
  }

  void __set_replicate_on_write(const bool val) {
    replicate_on_write = val;
    __isset.replicate_on_write = true;
  }

  void __set_key_validation_class(const std::string& val) {
    key_validation_class = val;
    __isset.key_validation_class = true;
  }

  void __set_key_alias(const std::string& val) {
    key_alias = val;
    __isset.key_alias = true;
  }

  void __set_compaction_strategy(const std::string& val) {
    compaction_strategy = val;
    __isset.compaction_strategy = true;
  }

  void __set_compaction_strategy_options(const std::map<std::string, std::string> & val) {
    compaction_strategy_options = val;
    __isset.compaction_strategy_options = true;
  }

  void __set_compression_options(const std::map<std::string, std::string> & val) {
    compression_options = val;
    __isset.compression_options = true;
  }

  void __set_bloom_filter_fp_chance(const double val) {
    bloom_filter_fp_chance = val;
    __isset.bloom_filter_fp_chance = true;
  }

  void __set_caching(const std::string& val) {
    caching = val;
    __isset.caching = true;
  }

  void __set_dclocal_read_repair_chance(const double val) {
    dclocal_read_repair_chance = val;
    __isset.dclocal_read_repair_chance = true;
  }

  void __set_populate_io_cache_on_flush(const bool val) {
    populate_io_cache_on_flush = val;
    __isset.populate_io_cache_on_flush = true;
  }

  void __set_memtable_flush_period_in_ms(const int32_t val) {
    memtable_flush_period_in_ms = val;
    __isset.memtable_flush_period_in_ms = true;
  }

  void __set_default_time_to_live(const int32_t val) {
    default_time_to_live = val;
    __isset.default_time_to_live = true;
  }

  void __set_index_interval(const int32_t val) {
    index_interval = val;
    __isset.index_interval = true;
  }

  void __set_speculative_retry(const std::string& val) {
    speculative_retry = val;
    __isset.speculative_retry = true;
  }

  void __set_triggers(const std::vector<TriggerDef> & val) {
    triggers = val;
    __isset.triggers = true;
  }

  void __set_row_cache_size(const double val) {
    row_cache_size = val;
    __isset.row_cache_size = true;
  }

  void __set_key_cache_size(const double val) {
    key_cache_size = val;
    __isset.key_cache_size = true;
  }

  void __set_row_cache_save_period_in_seconds(const int32_t val) {
    row_cache_save_period_in_seconds = val;
    __isset.row_cache_save_period_in_seconds = true;
  }

  void __set_key_cache_save_period_in_seconds(const int32_t val) {
    key_cache_save_period_in_seconds = val;
    __isset.key_cache_save_period_in_seconds = true;
  }

  void __set_memtable_flush_after_mins(const int32_t val) {
    memtable_flush_after_mins = val;
    __isset.memtable_flush_after_mins = true;
  }

  void __set_memtable_throughput_in_mb(const int32_t val) {
    memtable_throughput_in_mb = val;
    __isset.memtable_throughput_in_mb = true;
  }

  void __set_memtable_operations_in_millions(const double val) {
    memtable_operations_in_millions = val;
    __isset.memtable_operations_in_millions = true;
  }

  void __set_merge_shards_chance(const double val) {
    merge_shards_chance = val;
    __isset.merge_shards_chance = true;
  }

  void __set_row_cache_provider(const std::string& val) {
    row_cache_provider = val;
    __isset.row_cache_provider = true;
  }

  void __set_row_cache_keys_to_save(const int32_t val) {
    row_cache_keys_to_save = val;
    __isset.row_cache_keys_to_save = true;
  }

  bool operator == (const CfDef & rhs) const
  {
    if (!(keyspace == rhs.keyspace))
      return false;
    if (!(name == rhs.name))
      return false;
    if (__isset.column_type != rhs.__isset.column_type)
      return false;
    else if (__isset.column_type && !(column_type == rhs.column_type))
      return false;
    if (__isset.comparator_type != rhs.__isset.comparator_type)
      return false;
    else if (__isset.comparator_type && !(comparator_type == rhs.comparator_type))
      return false;
    if (__isset.subcomparator_type != rhs.__isset.subcomparator_type)
      return false;
    else if (__isset.subcomparator_type && !(subcomparator_type == rhs.subcomparator_type))
      return false;
    if (__isset.comment != rhs.__isset.comment)
      return false;
    else if (__isset.comment && !(comment == rhs.comment))
      return false;
    if (__isset.read_repair_chance != rhs.__isset.read_repair_chance)
      return false;
    else if (__isset.read_repair_chance && !(read_repair_chance == rhs.read_repair_chance))
      return false;
    if (__isset.column_metadata != rhs.__isset.column_metadata)
      return false;
    else if (__isset.column_metadata && !(column_metadata == rhs.column_metadata))
      return false;
    if (__isset.gc_grace_seconds != rhs.__isset.gc_grace_seconds)
      return false;
    else if (__isset.gc_grace_seconds && !(gc_grace_seconds == rhs.gc_grace_seconds))
      return false;
    if (__isset.default_validation_class != rhs.__isset.default_validation_class)
      return false;
    else if (__isset.default_validation_class && !(default_validation_class == rhs.default_validation_class))
      return false;
    if (__isset.id != rhs.__isset.id)
      return false;
    else if (__isset.id && !(id == rhs.id))
      return false;
    if (__isset.min_compaction_threshold != rhs.__isset.min_compaction_threshold)
      return false;
    else if (__isset.min_compaction_threshold && !(min_compaction_threshold == rhs.min_compaction_threshold))
      return false;
    if (__isset.max_compaction_threshold != rhs.__isset.max_compaction_threshold)
      return false;
    else if (__isset.max_compaction_threshold && !(max_compaction_threshold == rhs.max_compaction_threshold))
      return false;
    if (__isset.replicate_on_write != rhs.__isset.replicate_on_write)
      return false;
    else if (__isset.replicate_on_write && !(replicate_on_write == rhs.replicate_on_write))
      return false;
    if (__isset.key_validation_class != rhs.__isset.key_validation_class)
      return false;
    else if (__isset.key_validation_class && !(key_validation_class == rhs.key_validation_class))
      return false;
    if (__isset.key_alias != rhs.__isset.key_alias)
      return false;
    else if (__isset.key_alias && !(key_alias == rhs.key_alias))
      return false;
    if (__isset.compaction_strategy != rhs.__isset.compaction_strategy)
      return false;
    else if (__isset.compaction_strategy && !(compaction_strategy == rhs.compaction_strategy))
      return false;
    if (__isset.compaction_strategy_options != rhs.__isset.compaction_strategy_options)
      return false;
    else if (__isset.compaction_strategy_options && !(compaction_strategy_options == rhs.compaction_strategy_options))
      return false;
    if (__isset.compression_options != rhs.__isset.compression_options)
      return false;
    else if (__isset.compression_options && !(compression_options == rhs.compression_options))
      return false;
    if (__isset.bloom_filter_fp_chance != rhs.__isset.bloom_filter_fp_chance)
      return false;
    else if (__isset.bloom_filter_fp_chance && !(bloom_filter_fp_chance == rhs.bloom_filter_fp_chance))
      return false;
    if (__isset.caching != rhs.__isset.caching)
      return false;
    else if (__isset.caching && !(caching == rhs.caching))
      return false;
    if (__isset.dclocal_read_repair_chance != rhs.__isset.dclocal_read_repair_chance)
      return false;
    else if (__isset.dclocal_read_repair_chance && !(dclocal_read_repair_chance == rhs.dclocal_read_repair_chance))
      return false;
    if (__isset.populate_io_cache_on_flush != rhs.__isset.populate_io_cache_on_flush)
      return false;
    else if (__isset.populate_io_cache_on_flush && !(populate_io_cache_on_flush == rhs.populate_io_cache_on_flush))
      return false;
    if (__isset.memtable_flush_period_in_ms != rhs.__isset.memtable_flush_period_in_ms)
      return false;
    else if (__isset.memtable_flush_period_in_ms && !(memtable_flush_period_in_ms == rhs.memtable_flush_period_in_ms))
      return false;
    if (__isset.default_time_to_live != rhs.__isset.default_time_to_live)
      return false;
    else if (__isset.default_time_to_live && !(default_time_to_live == rhs.default_time_to_live))
      return false;
    if (__isset.index_interval != rhs.__isset.index_interval)
      return false;
    else if (__isset.index_interval && !(index_interval == rhs.index_interval))
      return false;
    if (__isset.speculative_retry != rhs.__isset.speculative_retry)
      return false;
    else if (__isset.speculative_retry && !(speculative_retry == rhs.speculative_retry))
      return false;
    if (__isset.triggers != rhs.__isset.triggers)
      return false;
    else if (__isset.triggers && !(triggers == rhs.triggers))
      return false;
    if (__isset.row_cache_size != rhs.__isset.row_cache_size)
      return false;
    else if (__isset.row_cache_size && !(row_cache_size == rhs.row_cache_size))
      return false;
    if (__isset.key_cache_size != rhs.__isset.key_cache_size)
      return false;
    else if (__isset.key_cache_size && !(key_cache_size == rhs.key_cache_size))
      return false;
    if (__isset.row_cache_save_period_in_seconds != rhs.__isset.row_cache_save_period_in_seconds)
      return false;
    else if (__isset.row_cache_save_period_in_seconds && !(row_cache_save_period_in_seconds == rhs.row_cache_save_period_in_seconds))
      return false;
    if (__isset.key_cache_save_period_in_seconds != rhs.__isset.key_cache_save_period_in_seconds)
      return false;
    else if (__isset.key_cache_save_period_in_seconds && !(key_cache_save_period_in_seconds == rhs.key_cache_save_period_in_seconds))
      return false;
    if (__isset.memtable_flush_after_mins != rhs.__isset.memtable_flush_after_mins)
      return false;
    else if (__isset.memtable_flush_after_mins && !(memtable_flush_after_mins == rhs.memtable_flush_after_mins))
      return false;
    if (__isset.memtable_throughput_in_mb != rhs.__isset.memtable_throughput_in_mb)
      return false;
    else if (__isset.memtable_throughput_in_mb && !(memtable_throughput_in_mb == rhs.memtable_throughput_in_mb))
      return false;
    if (__isset.memtable_operations_in_millions != rhs.__isset.memtable_operations_in_millions)
      return false;
    else if (__isset.memtable_operations_in_millions && !(memtable_operations_in_millions == rhs.memtable_operations_in_millions))
      return false;
    if (__isset.merge_shards_chance != rhs.__isset.merge_shards_chance)
      return false;
    else if (__isset.merge_shards_chance && !(merge_shards_chance == rhs.merge_shards_chance))
      return false;
    if (__isset.row_cache_provider != rhs.__isset.row_cache_provider)
      return false;
    else if (__isset.row_cache_provider && !(row_cache_provider == rhs.row_cache_provider))
      return false;
    if (__isset.row_cache_keys_to_save != rhs.__isset.row_cache_keys_to_save)
      return false;
    else if (__isset.row_cache_keys_to_save && !(row_cache_keys_to_save == rhs.row_cache_keys_to_save))
      return false;
    return true;
  }
  bool operator != (const CfDef &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const CfDef & ) const;

  //uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  //uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

void swap(CfDef &a, CfDef &b);

typedef struct _KsDef__isset
{
  _KsDef__isset() : strategy_options(false), replication_factor(false), durable_writes(true) {}
  bool strategy_options;
  bool replication_factor;
  bool durable_writes;
}
_KsDef__isset;


class KsDef
{
 public:

  static const char* ascii_fingerprint; // = "465E9A5DAAF09390BB3CB86E62BD37E8";
  static const uint8_t binary_fingerprint[16]; // = {0x46,0x5E,0x9A,0x5D,0xAA,0xF0,0x93,0x90,0xBB,0x3C,0xB8,0x6E,0x62,0xBD,0x37,0xE8};

  KsDef() : name(), strategy_class(), replication_factor(0), durable_writes(true) {
  }

  virtual ~KsDef() throw() {}

  std::string name;
  std::string strategy_class;
  std::map<std::string, std::string>  strategy_options;
  int32_t replication_factor;
  std::vector<CfDef>  cf_defs;
  bool durable_writes;

  _KsDef__isset __isset;

  void __set_name(const std::string& val) {
    name = val;
  }

  void __set_strategy_class(const std::string& val) {
    strategy_class = val;
  }

  void __set_strategy_options(const std::map<std::string, std::string> & val) {
    strategy_options = val;
    __isset.strategy_options = true;
  }

  void __set_replication_factor(const int32_t val) {
    replication_factor = val;
    __isset.replication_factor = true;
  }

  void __set_cf_defs(const std::vector<CfDef> & val) {
    cf_defs = val;
  }

  void __set_durable_writes(const bool val) {
    durable_writes = val;
    __isset.durable_writes = true;
  }

  bool operator == (const KsDef & rhs) const
  {
    if (!(name == rhs.name))
      return false;
    if (!(strategy_class == rhs.strategy_class))
      return false;
    if (__isset.strategy_options != rhs.__isset.strategy_options)
      return false;
    else if (__isset.strategy_options && !(strategy_options == rhs.strategy_options))
      return false;
    if (__isset.replication_factor != rhs.__isset.replication_factor)
      return false;
    else if (__isset.replication_factor && !(replication_factor == rhs.replication_factor))
      return false;
    if (!(cf_defs == rhs.cf_defs))
      return false;
    if (__isset.durable_writes != rhs.__isset.durable_writes)
      return false;
    else if (__isset.durable_writes && !(durable_writes == rhs.durable_writes))
      return false;
    return true;
  }
  bool operator != (const KsDef &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const KsDef & ) const;

  //uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  //uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


}}} // namespace

// vim: ts=4 sw=4 et
