#pragma once

#include <optional>
#include <string>

#include "curl.hpp"

namespace BSD_CORE {

  std::optional<std::string> parse_content_disposition(const std::string& value);

  std::string filename_from_url(const std::string& url);

  std::string sanitize_filename(std::string name);

  std::string resolve_filename(const ResponseHead& head);

} // namespace BSD_CORE