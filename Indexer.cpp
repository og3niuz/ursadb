#include "Indexer.h"

#include "Core.h"

Indexer::Indexer(const DatabaseSnapshot *snap, const std::vector<IndexType> &types)
        : snap(snap), types(types), builder(BuilderType::FLAT, types) {}

void Indexer::index(const std::string &target) {
    try {
        builder.index(target);
    } catch (empty_file_error &e) {
        std::cout << "empty file (skip): " << target << std::endl;
    } catch (file_open_error &e) {
        std::cout << "failed to open \"" << target << "\" (skip): " << e.what() << std::endl;
    } catch (invalid_filename_error &e) {
        std::cout << "illegal file name (skip): " << target << std::endl;
    }

    if (builder.must_spill()) {
        make_spill();
    }
}

std::vector<const OnDiskDataset *> Indexer::created_dataset_ptrs() {
    std::vector<const OnDiskDataset *> ptrs;

    for (auto &p : created_datasets) {
        ptrs.push_back(p.get());
    }

    return ptrs;
}

void Indexer::register_dataset(const std::string &dataset_name) {
    created_datasets.push_back(std::make_unique<OnDiskDataset>(snap->db_base, dataset_name));
}

void Indexer::remove_dataset(const OnDiskDataset *dataset_ptr) {
    for (auto it = created_datasets.begin(); it != created_datasets.end(); ) {
        if ((*it).get() == dataset_ptr) {
            (*it)->drop();
            it = created_datasets.erase(it);
        } else {
            ++it;
        }
    }
}

// TODO use two indexers at once for index command
void Indexer::make_spill() {
    std::cout << "new dataset" << std::endl;
    auto dataset_name = snap->allocate_name();
    builder.save(snap->db_base, dataset_name);
    register_dataset(dataset_name);
    bool stop = false;

    while (!stop) {
        std::vector<const OnDiskDataset *> candidates = OnDiskDataset::get_compact_candidates(created_dataset_ptrs());

        if (candidates.size() >= INDEXER_COMPACT_THRESHOLD) {
            std::cout << "merge stuff" << std::endl;
            std::string merged_name = snap->allocate_name();
            OnDiskDataset::merge(snap->db_base, merged_name, candidates, nullptr);
            for (const auto *ds : candidates) {
                remove_dataset(ds);
            }
            register_dataset(merged_name);
        } else {
            std::cout << "not going to merge" << std::endl;
            stop = true;
        }
    }

    builder = DatasetBuilder(BuilderType::FLAT, types);
}

OnDiskDataset *Indexer::force_compact() {
    if (!builder.empty()) {
        make_spill();
    }

    if (created_datasets.empty()) {
        throw std::runtime_error("forced to compact but no single file was indexed");
    }

    if (created_datasets.size() > 1) {
        std::vector<const OnDiskDataset *> candidates = created_dataset_ptrs();
        std::string merged_name = snap->allocate_name();
        OnDiskDataset::merge(snap->db_base, merged_name, candidates, nullptr);
        for (const auto *ds : candidates) {
            remove_dataset(ds);
        }
        register_dataset(merged_name);
    }

    return (*created_datasets.begin()).get();
}

std::vector<const OnDiskDataset *> Indexer::finalize() {
    if (!builder.empty()) {
        make_spill();
    }

    return created_dataset_ptrs();
}
