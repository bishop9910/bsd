#include "file_name.hpp"

#include "utils.hpp"

namespace BSD_CORE {

  std::optional<std::string>
  parse_content_disposition(const std::string& value) {
    if (auto p = value.find("filename*="); p != std::string::npos) {
      auto v = value.substr(p + 10);
      if (auto q = v.find(';'); q != std::string::npos) v = v.substr(0, q);
      if (auto sep = v.find("''"); sep != std::string::npos)
        return url_decode(v.substr(sep + 2));
      return url_decode(v);
    }
    if (auto p = value.find("filename="); p != std::string::npos) {
      auto v = value.substr(p + 9);
      if (auto q = v.find(';'); q != std::string::npos) v = v.substr(0, q);
      if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
        v = v.substr(1, v.size() - 2);
      return v;
    }
    return std::nullopt;
  }

  std::string filename_from_url(const std::string& url) {
    std::string path = url;
    if (auto p = path.find('?'); p != std::string::npos) path = path.substr(0, p);
    if (auto p = path.find('#'); p != std::string::npos) path = path.substr(0, p);
    auto slash = path.rfind('/');
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    return url_decode(name);
  }

  std::string sanitize_filename(std::string name) {
    if (auto p = name.find_last_of("/\\"); p != std::string::npos)
      name = name.substr(p + 1);

    for (char& c : name) {
      if (c == '\0' || c == ':' || c == '*' || c == '?' ||
          c == '"' || c == '<' || c == '>' || c == '|')
        c = '_';
    }

    if (name.empty() || name == "." || name == "..")
      name = "download";

    return name;
  }

  std::string resolve_filename(const ResponseHead& head) {
    if (!head.content_disposition.empty()) {
      if (auto n = parse_content_disposition(head.content_disposition))
        return sanitize_filename(*n);
    }

    if (auto n = filename_from_url(head.url); !n.empty())
      return sanitize_filename(n);

    return "index.html";
  }

} // namespace BSD_CORE