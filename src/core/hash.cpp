#include "hash.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <openssl/evp.h>

namespace BSD_CORE {

  ChecksumSpec parse_checksum(const std::string& s) {
    ChecksumSpec cs;
    if (s.empty()) return cs;

    auto colon = s.find(':');
    if (colon == std::string::npos)
      throw std::runtime_error("invalid checksum format (expected type:hash): " + s);

    cs.algo = s.substr(0, colon);
    cs.hash = s.substr(colon + 1);

    if (cs.algo.empty() || cs.hash.empty())
      throw std::runtime_error("invalid checksum format: " + s);

    auto to_lower = [](std::string& x) {
      std::transform(x.begin(), x.end(), x.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    };
    to_lower(cs.algo);
    to_lower(cs.hash);

    return cs;
  }

  bool is_supported_algo(const std::string& algo) {
    return EVP_get_digestbyname(algo.c_str()) != nullptr;
  }

  std::string compute_hash(const std::filesystem::path& p,
                          const std::string& algo)
  {
    const EVP_MD* md = EVP_get_digestbyname(algo.c_str());
    if (!md)
      throw std::runtime_error("unknown hash algo: " + algo);

    std::ifstream ifs(p, std::ios::binary);
    if (!ifs)
      throw std::runtime_error("cannot open file: " + p.string());

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
      throw std::runtime_error("EVP_MD_CTX_new failed");

    if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
      EVP_MD_CTX_free(ctx);
      throw std::runtime_error("EVP_DigestInit_ex failed");
    }

    std::vector<char> buf(1 << 20);   // 1MB
    while (ifs) {
      ifs.read(buf.data(), static_cast<std::streamsize>(buf.size()));
      std::streamsize n = ifs.gcount();
      if (n > 0) {
        if (EVP_DigestUpdate(ctx, buf.data(), static_cast<std::size_t>(n)) != 1) {
          EVP_MD_CTX_free(ctx);
          throw std::runtime_error("EVP_DigestUpdate failed");
        }
      }
    }

    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int  out_len = 0;
    if (EVP_DigestFinal_ex(ctx, out, &out_len) != 1) {
      EVP_MD_CTX_free(ctx);
      throw std::runtime_error("EVP_DigestFinal_ex failed");
    }
    EVP_MD_CTX_free(ctx);

    static const char* hex = "0123456789abcdef";
    std::string result;
    result.reserve(out_len * 2);
    for (unsigned int i = 0; i < out_len; ++i) {
      result.push_back(hex[out[i] >> 4]);
      result.push_back(hex[out[i] & 0x0F]);
    }
    return result;
  }

  bool verify_hash(const std::filesystem::path& p, const ChecksumSpec& spec) {
    if (spec.empty()) return true;
      return compute_hash(p, spec.algo) == spec.hash;
    }

  std::string md5_hex(const std::string& s) {
    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int  out_len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
    EVP_DigestUpdate(ctx, s.data(), s.size());
    EVP_DigestFinal_ex(ctx, out, &out_len);
    EVP_MD_CTX_free(ctx);

    static const char* hex = "0123456789abcdef";
    std::string r;
    r.reserve(out_len * 2);
    for (unsigned int i = 0; i < out_len; ++i) {
      r.push_back(hex[out[i] >> 4]);
      r.push_back(hex[out[i] & 0xF]);
    }
    return r;
  }

} // namespace BSD_CORE