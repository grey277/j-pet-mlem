#pragma once

// @cond PRIVATE
// redefine help formatting for greater readibility
namespace cmdline {

class string : public std::string {
 public:
  string() : std::string(), fn_ext(string::npos), fn_sep(string::npos) {}

  string(const char* s)
      : std::string(s),
        fn_ext(find_last_of(".")),
        fn_sep(find_last_of("\\/")) {}

  string(const std::string str)
      : std::string(str),
        fn_ext(find_last_of(".")),
        fn_sep(find_last_of("\\/")) {}

  string wo_ext() const {
    return substr(0,
                  fn_ext != std::string::npos &&
                          (fn_sep == std::string::npos || fn_sep < fn_ext)
                      ? fn_ext
                      : std::string::npos);
  }

  string wo_path() const {
    return substr(fn_sep != std::string::npos ? fn_sep + 1 : 0);
  }

  string ext() const {
    return substr(fn_ext != std::string::npos ? fn_ext : size(), size());
  }

 private:
  size_t fn_ext;
  size_t fn_sep;
};

namespace detail {
template <> inline std::string readable_typename<int>() { return "size"; }
template <> inline std::string readable_typename<long>() { return "seed"; }
template <> inline std::string readable_typename<double>() { return "float"; }
template <> inline std::string readable_typename<string>() { return "file"; }

template <> inline std::string default_value<double>(double def) {
  if (def == 0.)
    return "auto";
  return detail::lexical_cast<std::string>(def);
}
template <> inline std::string default_value<ssize_t>(ssize_t def) {
  if (def < 0)
    return "all";
  return detail::lexical_cast<std::string>(def);
}
}

}  // cmdline
// @end cond
