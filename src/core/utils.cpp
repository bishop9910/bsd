#include "utils.hpp"

namespace BSD_CORE {
  std::string versionToString(std::array<int, 3> version){
    std::string ver_s = "";
    for(int i=0; i<3; i++){
      ver_s.append(std::to_string(version[i]));
      if(i != 2){
        ver_s.append(".");
      }
    }
    return ver_s;
  }

  std::string trim(const std::string& s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    auto b = std::find_if(s.begin(), s.end(), not_space);
    auto e = std::find_if(s.rbegin(), s.rend(), not_space).base();
    return (b < e) ? std::string(b, e) : std::string{};
  }

  std::map<std::string, std::string, CaseInsensitiveLess> rawHeaderToMap(const std::vector<std::string>& raw_headers) {
    std::map<std::string, std::string, CaseInsensitiveLess> m;
    for (const auto& line : raw_headers) {
      auto colon = line.find(':');
      if (colon == std::string::npos) continue;   // 非法行，跳过
      std::string key   = trim(line.substr(0, colon));
      std::string value = trim(line.substr(colon + 1));
      if (key.empty()) continue;
      m[key] = value;
    }
    return m;
  }

  uuids::uuid_random_generator getRandomUuidGen() {
    std::random_device rd;
    auto seed_data = std::array<int, std::mt19937::state_size>{};
    std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
    std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
    std::mt19937 engine(seq);

    uuids::uuid_random_generator gen{engine};
    return gen;
  }

} // namespace BSD_CORE
