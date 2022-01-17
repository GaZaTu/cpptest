#include <source_location>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <concepts>

template <typename... Args>
std::string string_format(std::string_view format, Args... args) {
  int size_s = std::snprintf(nullptr, 0, format.data(), args...) + 1;
  if (size_s <= 0) {
    throw std::runtime_error("Error during formatting.");
  }
  auto size = static_cast<size_t>(size_s);
  auto buf = std::make_unique<char[]>(size);
  std::snprintf(buf.get(), size, format.data(), args...);
  return std::string{buf.get(), buf.get() + size - 1};
}

struct console {
public:
  enum log_type {
    LOG,
    WRN,
    ERR,
  };

  using handler_t = std::function<void(log_type, std::time_t, const std::source_location&, std::string_view)>;

  static log_type level;
  static handler_t handler;

  console(std::source_location l = std::source_location::current()) : _location(std::move(l)) {
  }

  template <typename... Args>
  static std::string make_logstr(log_type type, std::time_t now, const std::source_location& location, std::string_view format, Args... args) {
    constexpr char format_iso[] = "%Y-%m-%dT%H:%M:%SZ";
    constexpr char output_iso[] = "YYYY-mm-ddTHH:MM:SSZ";
    std::string now_str;
    now_str.resize(sizeof(output_iso));
    std::strftime(now_str.data(), sizeof(output_iso), format_iso, std::gmtime(&now));

    auto file_name = std::string_view{location.file_name()}.substr(sizeof(SOURCE_DIR));
    auto line = location.line();
    auto column = location.column();
    auto function_name = location.function_name();
    auto message = string_format(format, std::forward<Args>(args)...);

    auto logstr = string_format("[%s] |%s| %s(%d:%d) `%s`: %s", log_type_str[type], now_str.data(), file_name.data(),
        line, column, function_name, message.data());

    return logstr;
  }

  template <typename... Args>
  void log(log_type type, std::string_view format, Args... args) {
    if ((int)type < (int)level) {
      return;
    }

    auto now = std::time(nullptr);
    auto logstr = make_logstr(type, now, _location, format, std::forward<Args>(args)...);

    handler(type, now, _location, logstr);
  }

  template <typename F>
  void log(log_type type, F&& func) {
    if ((int)type < (int)level) {
      return;
    }

    log(type, func());
  }

  template <typename... Args>
  void log(std::string_view format, Args... args) {
    log(LOG, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void warn(std::string_view format, Args... args) {
    log(WRN, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void error(std::string_view format, Args... args) {
    log(ERR, format, std::forward<Args>(args)...);
  }

  static handler_t make_stream_handler(std::ostream& stream) {
    return [&stream](auto, auto, const auto&, auto logstr) {
      stream << logstr << std::endl;
    };
  }

  static handler_t make_file_handler(const std::filesystem::path& path) {
    return [path](auto, auto, const auto&, auto logstr) {
      std::ofstream stream{path, std::ios::app};
      stream << logstr << std::endl;
    };
  }

  static handler_t make_cout_handler() {
    return [](auto type, auto, const auto&, auto logstr) {
      auto stream = &std::cout;
      if (type != LOG) {
        stream = &std::cerr;
      }

      *stream << logstr << std::endl;
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
  };

  std::source_location _location;
};

console::log_type console::level = console::LOG;
console::handler_t console::handler = console::make_cout_handler();

template <std::derived_from<std::runtime_error> E = std::runtime_error>
class eformatter {
public:
  eformatter(std::source_location l = std::source_location::current()) : _location(std::move(l)) {}

  template <typename... Args>
  auto format(std::string_view format, Args... args) {
    auto now = std::time(nullptr);
    auto logstr = console::make_logstr(console::ERR, now, _location, format, std::forward<Args>(args)...);

    return E{logstr};
  }

private:
  std::source_location _location;
};
