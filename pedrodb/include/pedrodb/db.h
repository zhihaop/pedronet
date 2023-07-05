#ifndef PEDRODB_DB_H
#define PEDRODB_DB_H

#include "pedrodb/defines.h"
#include "pedrodb/options.h"
#include "pedrodb/status.h"

namespace pedrodb {

struct DB : pedrolib::noncopyable, pedrolib::nonmovable {
  static Status Open(const Options &options, const std::string &name,
                     std::shared_ptr<DB> *db);

  DB() = default;
  virtual ~DB() = default;

  virtual Status Get(const ReadOptions &options, std::string_view key,
                     std::string *value) = 0;

  virtual Status Put(const WriteOptions &options, std::string_view key,
                     std::string_view value) = 0;

  virtual Status Delete(const WriteOptions &options, std::string_view key) = 0;

  virtual Status Compact() = 0;
};
} // namespace pedrodb

#endif // PEDRODB_DB_H