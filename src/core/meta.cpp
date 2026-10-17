#include "meta.hpp"

#include <stdexcept>
#include <string>

namespace BSD_CORE {

  BS_JSON::Json to_json(const Meta& m) {
    BS_JSON::Json j = BS_JSON::Json::Object{};
    j["version"]       = m.version;
    j["url"]           = m.url;
    j["file_name"]     = m.file_name;
    j["total_bytes"]   = static_cast<int64_t>(m.total_bytes);
    j["etag"]          = m.etag;
    j["last_modified"] = m.last_modified;
    j["accept_ranges"] = m.accept_ranges;
    j["created_at"]    = m.created_at;

    BS_JSON::Json arr = BS_JSON::Json::Array{};
    for (const auto& c : m.chunks) {
      BS_JSON::Json jc = BS_JSON::Json::Object{};
      jc["begin"] = static_cast<int64_t>(c.begin);
      jc["end"]   = static_cast<int64_t>(c.end);
      jc["done"]  = static_cast<int64_t>(c.done_saved);
      arr.push_back(std::move(jc));
    }
    j["chunks"] = std::move(arr);

    return j;
  }

  Meta from_json(const BS_JSON::Json& j) {
    Meta m;

    if (!j.is_object())
      throw std::runtime_error("meta: root is not an object");

    auto get_str = [&](const char* key) -> std::string {
      if (!j.has(key)) return {};
      const auto& v = j[key];
      if (v.is_null()) return {};
      if (!v.is_string())
        throw std::runtime_error(std::string("meta: '") + key + "' is not a string");
      return v.as_string();
    };

    auto get_u64 = [&](const char* key, std::uint64_t def = 0) -> std::uint64_t {
      if (!j.has(key)) return def;
      const auto& v = j[key];
      if (v.is_null()) return def;
      if (!v.is_int() && !v.is_double())
        throw std::runtime_error(std::string("meta: '") + key + "' is not a number");
      const auto raw = v.as_int();
      if (raw < 0)
        throw std::runtime_error(std::string("meta: '") + key + "' is negative");
      return static_cast<std::uint64_t>(raw);
    };

    m.version       = j.has("version") ? static_cast<int>(get_u64("version")) : 0;
    m.url           = get_str("url");
    m.file_name     = get_str("file_name");
    m.total_bytes   = get_u64("total_bytes");
    m.etag          = get_str("etag");
    m.last_modified = get_str("last_modified");
    m.accept_ranges = get_str("accept_ranges");
    m.created_at    = get_str("created_at");

    if (m.total_bytes == 0)
      throw std::runtime_error("meta: total_bytes is zero");

    if (!j.has("chunks") || !j["chunks"].is_array())
      throw std::runtime_error("meta: missing chunks array");

    const auto& arr = j["chunks"].as_array();
    if (arr.empty())
      throw std::runtime_error("meta: chunks array is empty");

    std::uint64_t expected_begin = 0;
    m.chunks.reserve(arr.size());

    for (std::size_t i = 0; i < arr.size(); ++i) {
      const auto& jc = arr[i];
      const std::string idx = "[" + std::to_string(i) + "]";

      if (!jc.is_object())
        throw std::runtime_error("meta: chunk" + idx + " is not an object");

      auto chunk_u64 = [&](const char* key) -> std::uint64_t {
        if (!jc.has(key))
          throw std::runtime_error("meta: chunk" + idx + " missing '" + key + "'");
        const auto& v = jc[key];
        if (!v.is_int() && !v.is_double())
          throw std::runtime_error("meta: chunk" + idx + " '" + key + "' is not a number");
        const auto raw = v.as_int();
        if (raw < 0)
          throw std::runtime_error("meta: chunk" + idx + " '" + key + "' is negative");
        return static_cast<std::uint64_t>(raw);
      };

      Chunk c;
      c.begin       = chunk_u64("begin");
      c.end         = chunk_u64("end");
      c.done_saved  = chunk_u64("done");
      c.done = c.done_saved;

      if (c.begin != expected_begin) {
        throw std::runtime_error(
          "meta: chunk" + idx + " begin=" + std::to_string(c.begin) +
          " expected=" + std::to_string(expected_begin) + " (not contiguous)");
      }

      if (c.end < c.begin)
        throw std::runtime_error("meta: chunk" + idx + " end < begin");

      const std::uint64_t sz = c.end - c.begin + 1;
      if (c.done_saved > sz) {
        throw std::runtime_error(
          "meta: chunk" + idx + " done=" + std::to_string(c.done) +
          " > size=" + std::to_string(sz));
      }

      expected_begin = c.end + 1;
      if (expected_begin == 0)
        throw std::runtime_error("meta: chunk" + idx + " end overflow");

      m.chunks.push_back(c);
    }

    if (expected_begin != m.total_bytes) {
      throw std::runtime_error(
        "meta: chunks cover [0," + std::to_string(expected_begin) +
        ") but total_bytes=" + std::to_string(m.total_bytes));
    }

    return m;
  }

} // namespace BSD_CORE