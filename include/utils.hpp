#pragma once

#include <iostream>
#include <array>
#include <string>
#include <map>
#include <vector>
#include <random>
#include <chrono>
#include <filesystem>

#ifndef BSD_UTILS_
#define BSD_UTILS_

namespace BSD_CORE {
  std::string versionToString(const std::vector<int>& version, const int& build_version, const char* sep = ".");

  struct CaseInsensitiveLess {
    bool operator()(const std::string& a, const std::string& b) const {
      return std::lexicographical_compare(
        a.begin(), a.end(), b.begin(), b.end(),
        [](unsigned char x, unsigned char y) {
          return std::tolower(x) < std::tolower(y);
        });
    }
  };

  std::string trim(const std::string& s);

  std::map<std::string, std::string, CaseInsensitiveLess> rawHeaderToMap(const std::vector<std::string>& raw_headers);

  std::string human_size(double bytes);
  std::string human_time(double sec);

  uint64_t parse_size(const std::string& s);
  std::string url_decode(const std::string& s);

  std::string now_string();
  std::string http_time_now();

  std::filesystem::path PathFromUtf8(const std::string& s);
}

#endif