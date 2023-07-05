#ifndef PEDRODB_METADATA_MANAGER_H
#define PEDRODB_METADATA_MANAGER_H

#include "pedrodb/metadata_format.h"
#include "pedrodb/status.h"

#include <mutex>
#include <pedrolib/buffer/array_buffer.h>
#include <unordered_set>

namespace pedrodb {

class MetadataManager {
  mutable std::mutex mu_;
  uint32_t version_{};
  std::string name_;
  std::unordered_set<uint32_t> files_;

  File file_;

  const std::string path_;

  Status Recovery() {
    ArrayBuffer buffer(File::Size(file_));
    buffer.Append(&file_);

    MetadataHeader header;
    header.UnPack(&buffer);
    version_ = header.version;
    name_ = header.name;

    PEDRODB_INFO("read database {}", name_);

    while (buffer.ReadableBytes()) {
      MetadataChangeLogEntry logEntry;
      logEntry.UnPack(&buffer);

      if (logEntry.type == kCreateFile) {
        files_.emplace(logEntry.id);
      } else {
        files_.erase(logEntry.id);
      }
    }

    return Status::kOk;
  }

  Status CreateDatabase() {
    MetadataHeader header;
    header.name = name_;
    header.version = version_ = 0;

    ArrayBuffer buffer(MetadataHeader::SizeOf(name_.size()));
    header.Pack(&buffer);
    buffer.Retrieve(&file_);
    file_.Sync();
    return Status::kOk;
  }

public:
  explicit MetadataManager(std::string path) : path_(std::move(path)) {}
  ~MetadataManager() = default;

  Status Init() {
    size_t index = path_.find_last_of(".db");
    if (index != path_.size() - 1) {
      PEDRODB_ERROR("db filename[{}] error", path_);
      return Status::kInvalidArgument;
    }

    name_ = path_.substr(0, path_.size() - 3);

    File::OpenOption option{.mode = File::OpenMode::kReadWrite, .create = 0777};
    file_ = File::Open(path_.c_str(), option);
    if (!file_.Valid()) {
      PEDRODB_ERROR("cannot open filename[{}]: {}", path_, file_.GetError());
      return Status::kIOError;
    }

    int64_t filesize = File::Size(file_);
    if (filesize < 0) {
      PEDRODB_ERROR("cannot get filesize[{}]: {}", path_, file_.GetError());
      return Status::kIOError;
    }

    if (filesize != 0) {
      return Recovery();
    }

    PEDRODB_INFO("db[{}] not exist, create one", name_);
    return CreateDatabase();
  }

  auto AcquireLock() const noexcept { return std::unique_lock{mu_}; }

  uint32_t AcquireVersion() {
    PEDRODB_INFO("acquire timestamp");
    version_ += kBatchVersions;
    auto v = htobe(version_);
    file_.Pwrite(0, &v, sizeof(v));
    PEDRODB_IGNORE_ERROR(file_.Sync());
    return version_;
  }

  uint32_t GetCurrentVersion() const noexcept { return version_; }

  const auto &GetFiles() const noexcept { return files_; }

  Status CreateFile(uint32_t id) {
    if (files_.count(id)) {
      return Status::kOk;
    }
    files_.emplace(id);

    MetadataChangeLogEntry entry;
    entry.type = kCreateFile;
    entry.id = id;

    char buf[64];
    BufferSlice slice(buf, sizeof(buf));
    entry.Pack(&slice);
    slice.Retrieve(&file_);

    PEDRODB_IGNORE_ERROR(file_.Sync());
    return Status::kOk;
  }

  Status DeleteFile(uint32_t id) {
    auto it = files_.find(id);
    if (it == files_.end()) {
      return Status::kOk;
    }
    files_.erase(it);

    MetadataChangeLogEntry entry;
    entry.type = kDeleteFile;
    entry.id = id;

    char buf[64];
    BufferSlice slice(buf, sizeof(buf));
    entry.Pack(&slice);
    slice.Retrieve(&file_);

    PEDRODB_IGNORE_ERROR(file_.Sync());
    return Status::kOk;
  }

  std::string GetDataFilePath(uint32_t id) const noexcept {
    return fmt::format("{}_{}.data", name_, id);
  }
};

} // namespace pedrodb

#endif // PEDRODB_METADATA_MANAGER_H