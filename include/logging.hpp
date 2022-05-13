#if (defined(__GNUC__) && !defined(__clang__)) && (__GNUC__ < 11)
#include <experimental/source_location>
#else
#include <source_location>
#endif
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <concepts>

#if (defined(__GNUC__) && !defined(__clang__)) && (__GNUC__ < 11)
namespace std {
using source_location = std::experimental::source_location;
}
#endif

template <typename... Args>
std::string string_format(std::string_view format, Args&&... args) {
  int size_s = std::snprintf(nullptr, 0, format.data(), args...) + 1;
  if (size_s <= 0) {
    throw std::runtime_error("Error during formatting.");
  }
  auto size = static_cast<size_t>(size_s);
  auto buf = std::make_unique<char[]>(size);
  std::snprintf(buf.get(), size, format.data(), args...);
  return std::string{buf.get(), buf.get() + size - 1};
}

struct logger {
public:
  enum log_type {
    LOG,
    WRN,
    ERR,
    CRT,
  };

  using handler_t = std::function<void(log_type, std::time_t, const std::source_location&, std::string_view, std::string_view)>;

  static log_type level;
  static handler_t handler;

  logger(std::source_location l = std::source_location::current()) : _location(std::move(l)) {
  }

  ~logger() {
    std::string message = _ostream.str();
    if (message.empty()) {
      return;
    }

    log(_ostream_level, "%s", message.data());
  }

  static std::string local_time_to_utc_iso_string(std::time_t time) {
    constexpr char format_iso[] = "%Y-%m-%dT%H:%M:%SZ";
    constexpr char output_iso[] = "YYYY-mm-ddTHH:MM:SSZ";
    std::string time_str;
    time_str.resize(sizeof(output_iso));
    std::strftime(time_str.data(), sizeof(output_iso), format_iso, std::gmtime(&time));
    return time_str;
  }

  static std::string format_logstr(log_type type, std::time_t time, const std::source_location& location, std::string_view message) {
    auto type_str = std::string_view{log_type_str[type]};
    auto time_str = local_time_to_utc_iso_string(time);
    auto file_name = std::string_view{location.file_name()}.substr(sizeof(SOURCE_DIR));
    auto line = location.line();
    auto column = location.column();
    auto function_name = std::string_view{location.function_name()};

    auto logstr = string_format("[%s] |%s| %s:%d:%d `%s`: %s", type_str.data(), time_str.data(), file_name.data(),
        line, column, function_name.data(), message.data());

    return logstr;
  }

  template <typename... Args>
  void log(log_type type, std::string_view format, Args&&... args) {
    if ((int)type < (int)level) {
      return;
    }

    auto now = std::time(nullptr);
    auto message = string_format(format, std::forward<Args>(args)...);
    auto logstr = format_logstr(type, now, _location, message);

    handler(type, now, _location, message, logstr);
  }

  template <typename F>
  void log(log_type type, F&& func) {
    if ((int)type < (int)level) {
      return;
    }

    log(type, func());
  }

  template <typename... Args>
  inline void log(std::string_view format, Args&&... args) {
    log(LOG, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void warn(std::string_view format, Args&&... args) {
    log(WRN, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void error(std::string_view format, Args&&... args) {
    log(ERR, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline void critical(std::string_view format, Args&&... args) {
    log(CRT, format, std::forward<Args>(args)...);
  }

  inline logger& operator<<(log_type ostream_level) {
    _ostream_level = ostream_level;
    return *this;
  }

  template <typename Arg>
  inline logger& operator<<(Arg&& arg) {
    _ostream << std::forward<Arg>(arg);
    return *this;
  }

  static handler_t make_noop_handler() {
    return [](auto, auto, const auto&, auto, auto) {};
  }

  static handler_t make_stream_handler(std::ostream& stream) {
    return [&stream](auto, auto, const auto&, auto, auto logstr) {
      stream << logstr << '\n';
    };
  }

  static handler_t make_file_handler(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
      if (std::filesystem::file_size(path) > (1 * 1024 * 1024)) {
        std::filesystem::remove(path);
      }
    }

    return [path](auto, auto, const auto&, auto, auto logstr) {
      std::ofstream{path, std::ios::app} << logstr << '\n';
    };
  }

  static handler_t make_merged_handler(handler_t h0, handler_t h1) {
    return [h0{std::move(h0)}, h1{std::move(h1)}](auto type, auto time, const auto& location, auto message, auto logstr) {
      h0(type, time, location, message, logstr);
      h1(type, time, location, message, logstr);
    };
  }

private:
#ifdef PROJECT_SOURCE_DIR
  static constexpr char SOURCE_DIR[] = PROJECT_SOURCE_DIR;
#else
  static constexpr char SOURCE_DIR[] = "";
#endif

  static constexpr const char* log_type_str[] = {
      "LOG",
      "WRN",
      "ERR",
      "CRT",
  };

  std::source_location _location;

  std::ostringstream _ostream;
  log_type _ostream_level = LOG;
};

logger::log_type logger::level = logger::LOG;
logger::handler_t logger::handler = logger::make_stream_handler(std::cerr);

template <std::derived_from<std::runtime_error> E = std::runtime_error>
class eformatter {
public:
  eformatter(std::source_location l = std::source_location::current()) : _location(std::move(l)) {}

  template <typename... Args>
  auto format(std::string_view format, Args&&... args) {
    auto now = std::time(nullptr);
    auto message = string_format(format, std::forward<Args>(args)...);
    auto logstr = logger::format_logstr(logger::ERR, now, _location, message);

    return E{logstr};
  }

private:
  std::source_location _location;
};
