#include "meta.hpp"

#include <stdexcept>

namespace BSD_CORE {

  Json to_json(const Meta& m) {
    Json j = Json::Object{};
    j["version"]       = m.version;
    j["url"]           = m.url;
    j["file_name"]     = m.file_name;
    j["total_bytes"]   = static_cast<int64_t>(m.total_bytes);
    j["chunk_size"]    = static_cast<int64_t>(m.chunk_size);
    j["etag"]          = m.etag;
    j["last_modified"] = m.last_modified;
    j["accept_ranges"] = m.accept_ranges;
    j["created_at"]    = m.created_at;

    Json arr = Json::Array{};
    for (const auto& c : m.chunks) {
      Json jc = Json::Object{};
      jc["begin"] = static_cast<int64_t>(c.begin);
      jc["end"]   = static_cast<int64_t>(c.end);
      jc["done"]  = static_cast<int64_t>(c.done);
      arr.push_back(std::move(jc));
    }
    j["chunks"] = std::move(arr);

    return j;
  }

  Meta from_json(const Json& j) {
    Meta m;

    auto get_str = [&](const char* key) -> std::string {
      return j.has(key) ? j[key].as_string() : std::string{};
    };
    auto get_u64 = [&](const char* key) -> std::uint64_t {
      return j.has(key) ? static_cast<std::uint64_t>(j[key].as_int()) : 0;
    };

    m.version       = j.has("version") ? static_cast<int>(j["version"].as_int()) : 1;
    m.url           = get_str("url");
    m.file_name     = get_str("file_name");
    m.total_bytes   = get_u64("total_bytes");
    m.chunk_size    = get_u64("chunk_size");
    m.etag          = get_str("etag");
    m.last_modified = get_str("last_modified");
    m.accept_ranges = get_str("accept_ranges");
    m.created_at    = get_str("created_at");

    if (!j.has("chunks") || !j["chunks"].is_array())
      throw std::runtime_error("meta: missing chunks array");

    for (const auto& jc : j["chunks"].as_array()) {
      Chunk c;
      c.begin = static_cast<std::uint64_t>(jc["begin"].as_int());
      c.end   = static_cast<std::uint64_t>(jc["end"].as_int());
      c.done  = static_cast<std::uint64_t>(jc["done"].as_int());
      m.chunks.push_back(c);
    }

    return m;
  }

} // namespace BSD_CORE