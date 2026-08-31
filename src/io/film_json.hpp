#pragma once

#include <nlohmann/json_fwd.hpp>

#include "pfdb/film.hpp"

namespace pfdb {

/// Serialize a film to a JSON object. This is the canonical machine-readable
/// representation the CLI emits under `--json`, designed to be stable and
/// pipe-friendly for power-user scripting.
nlohmann::json to_json(const Film& film);

}  // namespace pfdb
