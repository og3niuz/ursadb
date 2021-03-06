#pragma once

#include "Core.h"
#include "Database.h"
#include "MemMap.h"
#include "Query.h"
#include "Task.h"

#include <string>
#include <vector>

struct IndexMergeHelper;

class OnDiskIndex {
    const uint64_t *run_offsets;
    MemMap disk_map;
    std::string fname;
    IndexType ntype;

    const uint8_t *data() const { return disk_map.data(); }
    static constexpr uint32_t VERSION = 6;
    std::vector<FileId> query_primitive(TriGram trigram) const;
    bool internal_expand(QString::const_iterator qit, uint8_t *out, size_t pos, size_t comb_len,
                         const TrigramGenerator &gen, QueryResult &res) const;
    QueryResult expand_wildcards(const QString &qstr, size_t len, const TrigramGenerator &gen) const;

  public:
    explicit OnDiskIndex(const std::string &fname);

    const std::string &get_fname() const { return fname; }
    IndexType index_type() const { return ntype; }
    QueryResult query_str(const QString &str) const;
    static void on_disk_merge(
            const fs::path &db_base, const std::string &fname, IndexType merge_type,
            const std::vector<IndexMergeHelper> &indexes, Task *task);
};

struct IndexMergeHelper {
    const OnDiskIndex *index;
    uint32_t file_count;

    IndexMergeHelper(const OnDiskIndex *index, uint32_t file_count)
        : index(index), file_count(file_count) {}
};