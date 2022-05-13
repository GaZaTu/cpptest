#pragma once

#define COMBINATORS_LOGGING 1

#include <functional>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#ifdef COMBINATORS_LOGGING
#include <iostream>
#endif

namespace comb {
namespace detail {
template <auto Start, auto End, auto Inc, class F>
constexpr void constexpr_for(F&& f) {
  if constexpr (Start < End) {
    if (!f(std::integral_constant<decltype(Start), Start>())) {
      return;
    }

    constexpr_for<Start + Inc, End, Inc>(f);
  }
}

template <typename T0, typename T1>
struct flat_tuple {
  using type = std::tuple<T0, T1>;

  static inline type make(T0&& a0, T1&& a1) {
    return type{std::move(a0), std::move(a1)};
  }
};

template <typename... Vs0, typename... Vs1>
struct flat_tuple<std::tuple<Vs0...>, std::tuple<Vs1...>> {
  using type = std::tuple<Vs0..., Vs1...>;

  static inline type make(std::tuple<Vs0...>&& a0, std::tuple<Vs1...>&& a1) {
    return std::tuple_cat(a0, a1);
  }
};

template <typename... Vs0, typename T1>
struct flat_tuple<std::tuple<Vs0...>, T1> {
  using type = std::tuple<Vs0..., T1>;

  static inline type make(std::tuple<Vs0...>&& a0, T1&& a1) {
    return std::apply([&a1](auto&&... args) { return std::make_tuple(std::move(args)..., std::move(a1)); }, a0);
  }
};

template <typename T0, typename... Vs1>
struct flat_tuple<T0, std::tuple<Vs1...>> {
  using type = std::tuple<T0, Vs1...>;

  static inline type make(T0&& a0, std::tuple<Vs1...>&& a1) {
    return std::apply([&a0](auto&&... args) { return std::make_tuple(std::move(a0), std::move(args)...); }, a1);
  }
};

template <typename T0, typename T1>
struct flat_variant {
  using type = std::variant<std::monostate, T0, T1>;

  static inline type make1(T0&& a0) {
    type result;
    result.template emplace<1>(std::move(a0));
    return result;
  }

  static inline type make2(T1&& a1) {
    type result;
    result.template emplace<2>(std::move(a1));
    return result;
  }
};

template <typename... Vs0, typename... Vs1>
struct flat_variant<std::variant<std::monostate, Vs0...>, std::variant<std::monostate, Vs1...>> {
  using type = std::variant<std::monostate, Vs0..., Vs1...>;

  using variant0 = std::variant<std::monostate, Vs0...>;
  using variant1 = std::variant<std::monostate, Vs1...>;

  static inline type make1(variant0&& a0) {
    type result;
    comb::detail::constexpr_for<(size_t)0, std::variant_size_v<variant0>, (size_t)1>([&result, &a0](auto i) {
      if (a0.index() != i) {
        return true;
      }

      result.template emplace<i>(std::move(std::get<i>(a0)));
      return false;
    });
    return result;
  }

  static inline type make2(variant1&& a1) {
    type result;
    comb::detail::constexpr_for<(size_t)0, std::variant_size_v<variant1>, (size_t)1>([&result, &a1](auto i) {
      if (a1.index() != i) {
        return true;
      }

      if constexpr (i != 0) {
        result.template emplace<i + std::variant_size_v<variant0> - 1>(std::move(std::get<i>(a1)));
      }
      return false;
    });
    return result;
  }
};

template <typename... Vs0, typename T1>
struct flat_variant<std::variant<std::monostate, Vs0...>, T1> {
  using type = std::variant<std::monostate, Vs0..., T1>;

  using variant0 = std::variant<std::monostate, Vs0...>;

  static inline type make1(variant0&& a0) {
    type result;
    comb::detail::constexpr_for<(size_t)0, std::variant_size_v<variant0>, (size_t)1>([&result, &a0](auto i) {
      if (a0.index() != i) {
        return true;
      }

      result.template emplace<i>(std::move(std::get<i>(a0)));
      return false;
    });
    return result;
  }

  static inline type make2(T1&& a1) {
    type result;
    result.template emplace<std::variant_size_v<variant0>>(std::move(a1));
    return result;
  }
};

template <typename T0, typename... Vs1>
struct flat_variant<T0, std::variant<std::monostate, Vs1...>> {
  using type = std::variant<std::monostate, T0, Vs1...>;

  using variant1 = std::variant<std::monostate, Vs1...>;

  static inline type make1(T0&& a0) {
    type result;
    result.template emplace<1>(std::move(a0));
    return result;
  }

  static inline type make2(variant1&& a1) {
    type result;
    comb::detail::constexpr_for<(size_t)0, std::variant_size_v<variant1>, (size_t)1>([&result, &a1](auto i) {
      if (a1.index() != i) {
        return true;
      }

      if constexpr (i != 0) {
        result.template emplace<i + 1>(std::move(std::get<i>(a1)));
      }
      return false;
    });
    return result;
  }
};

template <typename T0>
struct flat_variant1 {
  using type = std::variant<std::monostate, T0>;

  static inline type make0() {
    return {};
  }

  static inline type make1(T0&& a0) {
    type result;
    result.template emplace<1>(std::move(a0));
    return result;
  }
};

template <typename... Vs0>
struct flat_variant1<std::variant<std::monostate, Vs0...>> {
  using type = std::variant<std::monostate, Vs0...>;

  using variant0 = std::variant<std::monostate, Vs0...>;

  static inline type make0() {
    return {};
  }

  static inline type make1(variant0&& a0) {
    return a0;
  }
};
} // namespace detail

struct context {
public:
  std::string source;
  uint64_t index = 0;
  uint32_t line = 0;
  uint32_t column = 0;

  comb::context move(uint32_t by) const {
    return {
        .source = source,
        .index = index + by,
    };
  }

  std::string_view view() const {
    return source;
  }

  std::string_view slice() const {
    return view().substr(index);
  }
};

template <typename T>
struct result {
public:
  using value_type = std::conditional_t<std::is_same_v<T, std::nullopt_t>, bool, T>;

  bool ok;
  std::optional<value_type> value;
  std::string expectation;
  comb::context ctx;
};

template <typename R>
struct decay_result {
  using type = R;
};

template <typename T>
struct decay_result<comb::result<T>> {
  using type = T;
};

template <typename T>
struct parser {
public:
  using value_type = T;
  using fn_type = std::function<comb::result<T>(const comb::context&)>;

  template <typename F>
  parser(F fn) : _fn{fn} {
  }

  // template <typename F>
  // parser(F fn, std::string_view name) : _fn{fn}, _name{name} {
  // }

  comb::result<T> operator()(const comb::context& ctx) const {
    return _fn(ctx);
  }

  std::string& name() {
    return _name;
  }

  const std::string& name() const {
    return _name;
  }

  comb::parser<T> named(std::string_view name) {
    using value_type = T;
    using parser_type = comb::parser<value_type>;

    std::string parser_name = (std::string)name;
    parser_type parser = std::move(*this);
    parser.name() = parser_name;

    return parser;
  }

  comb::parser<std::nullopt_t> operator!() {
    using value_type = std::nullopt_t;
    using parser_type = comb::parser<value_type>;

    std::string parser_name = "!" + _name;
    parser_type parser = [self{std::move(*this)}, parser_name](const comb::context& ctx) -> comb::result<value_type> {
          auto r1 = self(ctx);
          if (!r1.ok) {
            return success<value_type>(r1.ctx, std::nullopt);
          }

          return failure<value_type>(r1.ctx, parser_name);
        };
    parser.name() = parser_name;

    return parser;
  }

  comb::parser<std::optional<T>> optional() {
    using value_type = std::optional<T>;
    using parser_type = comb::parser<value_type>;

    std::string parser_name = "((" + _name + ") || null)";
    parser_type parser = [self{std::move(*this)}](const comb::context& ctx) -> comb::result<value_type> {
          auto r1 = self(ctx);
          if (!r1.ok) {
            return success<value_type>(r1.ctx, std::nullopt);
          }

          return success<value_type>(r1.ctx, std::move(*r1.value));
        };
    parser.name() = parser_name;

    return parser;
  }

  comb::parser<T> optional(T&& default_value) {
    using value_type = std::optional<T>;
    using parser_type = comb::parser<value_type>;

    std::string parser_name = "((" + _name + ") || default)";
    parser_type parser = [self{std::move(*this)}, default_value{std::move(default_value)}](const comb::context& ctx) -> comb::result<value_type> {
          auto r1 = self(ctx);
          if (!r1.ok) {
            return success<value_type>(r1.ctx, std::move(default_value));
          }

          return success<value_type>(r1.ctx, std::move(*r1.value));
        };
    parser.name() = parser_name;

    return parser;
  }

  comb::parser<std::vector<T>> atleast(int times) {
    using value_type = std::vector<T>;
    using parser_type = comb::parser<value_type>;

    std::string parser_name = "((" + _name + ") >= " + std::to_string(times) + " times)";
    parser_type parser = [self{std::move(*this)}, times, parser_name](const comb::context& ctx) -> comb::result<value_type> {
          value_type value;
          std::optional<comb::result<T>> r1;

          while (true) {
            r1 = self(r1 ? r1->ctx : ctx);
            if (!r1->ok) {
              break;
            }

            value.push_back(std::move(*r1->value));
          }

          if (value.size() < times) {
            return failure<value_type>(r1->ctx, r1->expectation);
          }

          return success<value_type>(r1->ctx, std::move(value));
        };
    parser.name() = parser_name;

    return parser;
  }

  template <typename T2>
  comb::parser<std::vector<T>> atleast(int times, comb::parser<T2>&& separator) {
    using value_type = std::vector<T>;
    using parser_type = comb::parser<value_type>;

    std::string parser_name = "((" + _name + ") >= " + std::to_string(times) + " times)";
    parser_type parser = [self{std::move(*this)}, times, parser_name, separator{std::move(separator)}](const comb::context& ctx) -> comb::result<value_type> {
          value_type value;
          std::optional<comb::result<T>> r1;
          std::optional<comb::result<T2>> r2;

          while (true) {
            r1 = self(r2 ? r2->ctx : ctx);
            if (!r1->ok) {
              break;
            }

            value.push_back(std::move(*r1->value));

            r2 = separator(r1->ctx);
            if (!r2->ok) {
              break;
            }
          }

          if (value.size() < times) {
            return failure<value_type>(r1->ctx, r1->expectation);
          }

          return success<value_type>(r1->ctx, std::move(value));
        };
    parser.name() = parser_name;

    return parser;
  }

  comb::parser<T> operator+(comb::parser<std::nullopt_t>&& other) {
    using value_type = T;
    using parser_type = comb::parser<value_type>;

    std::string parser_name = _name + " + " + other.name();
    parser_type parser = [self{std::move(*this)}, other{std::move(other)}](const comb::context& ctx) -> comb::result<value_type> {
          auto r1 = self(ctx);
          if (!r1.ok) {
            return failure<value_type>(r1.ctx, r1.expectation);
          }

          auto r2 = other(r1.ctx);
          if (!r2.ok) {
            return failure<value_type>(r2.ctx, r2.expectation);
          }

          if constexpr (std::is_same_v<T, std::nullopt_t>) {
            return success<value_type>(r2.ctx, std::nullopt);
          } else {
            return success<value_type>(r2.ctx, std::move(*r1.value));
          }
        };
    parser.name() = parser_name;

    return parser;
  }

  template <typename T2>
  comb::parser<std::conditional_t<std::is_same_v<T, std::nullopt_t>, T2, typename comb::detail::flat_tuple<T, T2>::type>> operator+(comb::parser<T2>&& other) {
    if constexpr (std::is_same_v<T, std::nullopt_t>) {
      using value_type = T2;
      using parser_type = comb::parser<value_type>;

      std::string parser_name = _name + " + " + other.name();
      parser_type parser = [self{std::move(*this)}, other{std::move(other)}](const comb::context& ctx) -> comb::result<value_type> {
            auto r1 = self(ctx);
            if (!r1.ok) {
              return failure<value_type>(r1.ctx, r1.expectation);
            }

            auto r2 = other(r1.ctx);
            if (!r2.ok) {
              return failure<value_type>(r2.ctx, r2.expectation);
            }

            return r2;
          };
      parser.name() = parser_name;

      return parser;
    } else {
      using value_helper = comb::detail::flat_tuple<T, T2>;
      using value_type = typename value_helper::type;
      using parser_type = comb::parser<value_type>;

      std::string parser_name = _name + " + " + other.name();
      parser_type parser = [self{std::move(*this)}, other{std::move(other)}](const comb::context& ctx) -> comb::result<value_type> {
            auto r1 = self(ctx);
            if (!r1.ok) {
              return failure<value_type>(r1.ctx, r1.expectation);
            }

            auto r2 = other(r1.ctx);
            if (!r2.ok) {
              return failure<value_type>(r2.ctx, r2.expectation);
            }

            value_type value = value_helper::make(std::move(*r1.value), std::move(*r2.value));
            return success<value_type>(r2.ctx, std::move(value));
          };
      parser.name() = parser_name;

      return parser;
    }
  }

  comb::parser<typename comb::detail::flat_variant1<T>::type> operator||(std::nullopt_t) {
    using value_helper = comb::detail::flat_variant1<T>;
    using value_type = typename value_helper::type;
    using parser_type = comb::parser<value_type>;

    std::string parser_name = "((" + _name + ") || null)";
    parser_type parser = [self{std::move(*this)}](const comb::context& ctx) -> comb::result<value_type> {
          auto r1 = self(ctx);
          if (!r1.ok) {
            value_type value = value_helper::make0();
            return success<value_type>(r1.ctx, std::move(value));
          }

          value_type value = value_helper::make1(std::move(*r1.value));
          return success<value_type>(r1.ctx, std::move(value));
        };
    parser.name() = parser_name;

    return parser;
  }

  template <typename T2>
  comb::parser<typename comb::detail::flat_variant<T, T2>::type> operator||(comb::parser<T2>&& other) {
    using value_helper = comb::detail::flat_variant<T, T2>;
    using value_type = typename value_helper::type;
    using parser_type = comb::parser<value_type>;

    std::string parser_name = _name + " || " + other.name();
    parser_type parser = [self{std::move(*this)}, other{std::move(other)}](
               const comb::context& ctx) -> comb::result<value_type> {
      auto r1 = self(ctx);
      if (r1.ok) {
        value_type value = value_helper::make1(std::move(*r1.value));
        return success<value_type>(r1.ctx, std::move(value));
      }

      auto r2 = other(r1.ctx);
      if (r2.ok) {
        value_type value = value_helper::make2(std::move(*r2.value));
        return success<value_type>(r2.ctx, std::move(value));
      }

      return failure<value_type>(r1.ctx, r1.expectation + " || " + r2.expectation);
    };
    parser.name() = parser_name;

    return parser;
  }

  template <typename F>
  comb::parser<typename comb::decay_result<std::invoke_result_t<F, const comb::context&, T&&>>::type> map(F&& map) {
    using value_type = typename comb::decay_result<std::invoke_result_t<F, const comb::context&, T&&>>::type;
    using parser_type = comb::parser<value_type>;

    std::string parser_name = _name;
    parser_type parser = [self{std::move(*this)}, map{std::move(map)}](const comb::context& ctx) -> comb::result<value_type> {
          auto r1 = self(ctx);
          if (!r1.ok) {
            return failure<value_type>(r1.ctx, r1.expectation);
          }

          return map(r1.ctx, std::move(*r1.value));
        };
    parser.name() = parser_name;

    return parser;
  }

  comb::parser<std::nullopt_t> discard() {
    using value_type = std::nullopt_t;
    using parser_type = comb::parser<value_type>;

    std::string parser_name = _name;
    parser_type parser = [self{std::move(*this)}, parser_name](const comb::context& ctx) -> comb::result<value_type> {
          auto r1 = self(ctx);
          if (!r1.ok) {
            return failure<value_type>(r1.ctx, parser_name);
          }

          return success<value_type>(r1.ctx, std::nullopt);
        };
    parser.name() = parser_name;

    return parser;
  }

private:
  fn_type _fn;
  std::string _name;
};

// void log(const comb::context& ctx, std::ostream& out) {
//   out << std::endl;
// }

// template <typename A0, typename... Args>
// void log(const comb::context& ctx, std::ostream& out, A0&& a0, Args&&... args) {
//   out << ' ' << a0;
//   log(ctx, out, std::forward<Args>(args)...);
// }

// template <typename... Args>
// void log(const comb::context& ctx, Args&&... args) {
// #ifdef COMBINATORS_LOGGING
//   std::cerr << '[' << ctx.index << ']' << ' ' << ctx.slice().substr(0, 20) << "...";
//   log(ctx, std::cerr, std::forward<Args>(args)...);
// #endif
// }

template <typename T>
comb::result<T> success(const comb::context& ctx, T&& value) {
  return {
      .ok = true,
      .value = std::move(value),
      .ctx = ctx,
  };
}

template <typename T>
comb::result<T> success(const comb::context& ctx, const T& value) {
  return {
      .ok = true,
      .value = value,
      .ctx = ctx,
  };
}

template <typename T>
comb::result<T> failure(const comb::context& ctx, std::string_view expectation) {
  return {
      .ok = false,
      .expectation = (std::string)expectation,
      .ctx = ctx,
  };
}

class parsing_error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

template <typename T>
T parse(std::string_view source, const comb::parser<T>& parser) {
  auto res = parser({.source = (std::string)source});
  if (!res.ok) {
    throw comb::parsing_error{"expected " + res.expectation + " at " + std::to_string(res.ctx.index)};
  }

  return *res.value;
}

// template <typename T>
// comb::parser<T> def(T&& def) {
//   using value_type = T;

//   return [def{std::move(def)}](const comb::context& ctx) -> comb::result<value_type> {
//     return success<value_type>(ctx, def);
//   };
// }

comb::parser<char> chr(char chr) {
  using value_type = char;
  using parser_type = comb::parser<value_type>;

  parser_type parser = [chr](const comb::context& ctx) -> comb::result<value_type> {
    if (ctx.slice()[0] == chr) {
      return success<value_type>(ctx.move(1), chr);
    } else {
      return failure<value_type>(ctx, std::string{chr});
    }
  };
  parser.name() = std::string{chr};

  return parser;
}

comb::parser<std::string_view> str(std::string_view str) {
  using value_type = std::string_view;
  using parser_type = comb::parser<value_type>;

  parser_type parser = [str](const comb::context& ctx) -> comb::result<value_type> {
    if (ctx.slice().starts_with(str)) {
      return success<value_type>(ctx.move(str.length()), ctx.slice().substr(0, str.length()));
    } else {
      return failure<value_type>(ctx, (std::string)str);
    }
  };
  parser.name() = (std::string)str;

  return parser;
}

comb::parser<char> charis(const char* expectation, bool (*charis)(char __c, const std::locale& __loc)) {
  using value_type = char;
  using parser_type = comb::parser<value_type>;

  parser_type parser = [expectation, charis](const comb::context& ctx) -> comb::result<value_type> {
    auto chr = ctx.slice()[0];
    auto locale = std::locale{};

    if (charis(chr, locale)) {
      return success<value_type>(ctx.move(1), chr);
    } else {
      return failure<value_type>(ctx, expectation);
    }
  };
  parser.name() = expectation;

  return parser;
}

comb::parser<char> alnum() {
  return charis("alnum", std::isalnum);
}

comb::parser<char> alpha() {
  return charis("alpha", std::isalpha);
}

comb::parser<char> blank() {
  return charis("blank", std::isblank);
}

comb::parser<char> cntrl() {
  return charis("cntrl", std::iscntrl);
}

comb::parser<char> digit() {
  return charis("digit", std::isdigit);
}

comb::parser<char> graph() {
  return charis("graph", std::isgraph);
}

comb::parser<char> lower() {
  return charis("lower", std::islower);
}

comb::parser<char> print() {
  return charis("print", std::isprint);
}

comb::parser<char> punct() {
  return charis("punct", std::ispunct);
}

comb::parser<char> space() {
  return charis("space", std::isspace);
}

comb::parser<char> upper() {
  return charis("upper", std::isupper);
}

comb::parser<char> xdigit() {
  return charis("xdigit", std::isxdigit);
}

auto tostring() {
  return [](const comb::context& ctx, std::vector<char>&& value) {
    return success(ctx, ctx.view().substr(ctx.index - value.size(), value.size()));
  };
}

// auto todouble() {
//   return [](const comb::context& ctx, std::string&& value) {
//     return success<double>(ctx, std::stod(value));
//   };
// }

comb::parser<std::string_view> blankstr(int minlen = 0) {
  return comb::blank().atleast(minlen).map(tostring());
}

comb::parser<std::string_view> identifier() {
  return comb::alnum().atleast(1).map(tostring());
}

comb::parser<double> decimal() {
  using value_type = double;

  auto parser0 = (
    (comb::chr('+') || comb::chr('-') || std::nullopt) +
    (comb::digit() || comb::chr('.')).atleast(1)
  );
  using parser0_value_type = typename decltype(parser0)::value_type;

  auto parser1 = parser0.map([](const comb::context& ctx, parser0_value_type&& value) {
    auto& [sign, digits] = value;
    bool isfloat = false;

    std::string str;
    switch (sign.index()) {
      case 1:
        str += std::get<1>(sign);
        break;
      case 2:
        str += std::get<2>(sign);
        break;
    }
    for (auto& digit : digits) {
      switch (digit.index()) {
        case 1:
          str += std::get<1>(digit);
          break;
        case 2:
          if (isfloat) {
            return failure<value_type>(ctx, "!.");
          }
          isfloat = true;
          str += std::get<2>(digit);
          break;
      }
    }

    return success<value_type>(ctx, std::stod(str));
  });
  using parser1_value_type = typename decltype(parser1)::value_type;

  return parser1;
}
} // namespace comb
