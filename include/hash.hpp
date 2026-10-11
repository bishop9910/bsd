#pragma once

#include <filesystem>
#include <string>

namespace BSD_CORE {

  struct ChecksumSpec {
    std::string algo;
    std::string hash;
    bool empty() const { return algo.empty(); }
  };

  ChecksumSpec parse_checksum(const std::string& s);

  std::string compute_hash(const std::filesystem::path& p,
                          const std::string& algo);

  bool verify_hash(const std::filesystem::path& p,
                  const ChecksumSpec& spec);

  bool is_supported_algo(const std::string& algo);
  std::string md5_hex(const std::string& s);

} // namespace BSD_CORE