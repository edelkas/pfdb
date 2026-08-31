#include "model/collection_model.hpp"

#include <utility>

#include "db/repository.hpp"

namespace pfdb {

CollectionModel::CollectionModel(std::vector<Film> films) : films_(std::move(films)) {
    rebuild_index();
}

CollectionModel CollectionModel::load(const db::Repository& repo) {
    return CollectionModel(repo.load_all());
}

void CollectionModel::rebuild_index() {
    index_.clear();
    index_.reserve(films_.size());
    for (std::size_t i = 0; i < films_.size(); ++i) {
        index_[films_[i].id] = i;
    }
}

const Film* CollectionModel::find(Id id) const noexcept {
    auto it = index_.find(id);
    if (it == index_.end()) {
        return nullptr;
    }
    return &films_[it->second];
}

std::vector<const Film*> CollectionModel::filter(const Predicate& pred) const {
    std::vector<const Film*> out;
    for (const auto& film : films_) {
        if (pred(film)) {
            out.push_back(&film);
        }
    }
    return out;
}

void CollectionModel::upsert(Film film) {
    auto it = index_.find(film.id);
    if (it != index_.end()) {
        films_[it->second] = std::move(film);
        return;
    }
    index_[film.id] = films_.size();
    films_.push_back(std::move(film));
}

bool CollectionModel::erase(Id id) {
    auto it = index_.find(id);
    if (it == index_.end()) {
        return false;
    }
    const std::size_t pos = it->second;
    const std::size_t last = films_.size() - 1;
    if (pos != last) {
        films_[pos] = std::move(films_[last]);
        index_[films_[pos].id] = pos;
    }
    films_.pop_back();
    index_.erase(id);
    return true;
}

}  // namespace pfdb
