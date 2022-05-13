#pragma once

#include "./combinators.hpp"
#include <iostream>

auto graphql_operation_type() {
  return (
    comb::str("query") ||
    comb::str("mutation") ||
    comb::str("subscription")
  );
}

auto graphql_name() {
  return comb::identifier();
}

auto graphql_value() {
  return comb::identifier(); // TODO
}

struct graphql_argument_t {
public:
  std::string_view name;
  std::string_view value;
};

auto graphql_argument() {
  auto parser = (
    graphql_name() +
    comb::blankstr(0).discard() +
    comb::chr(':').discard() +
    comb::blankstr(0).discard() +
    graphql_value()
  );
  using value_type = typename decltype(parser)::value_type;

  return parser.map([](const comb::context& ctx, value_type&& value) {
    auto& [_name, _value] = value;
    return comb::success(ctx, graphql_argument_t{
      .name = _name,
      .value = _value,
    });
  });
}

auto graphql_arguments() {
  return (
    comb::chr('(').discard() +
    comb::blankstr(0).discard() +
    graphql_argument().atleast(1, (comb::blankstr(0) + comb::chr(',') + comb::blankstr(0))) +
    comb::blankstr(0).discard() +
    comb::chr(')').discard()
  );
}

// auto graphql_field() {
//   return (
//     // graphql_alias().optional() +
//     graphql_name() +
//     (comb::blankstr(1) + graphql_arguments()).optional() +
//     (comb::blankstr(1) + graphql_directives()).optional() +
//     (comb::blankstr(1) + graphql_selection_set()).optional()
//   );
// }

// auto graphql_selection() {
//   return (
//     graphql_field() ||
//     graphql_fragment_spread() ||
//     graphql_inline_fragment()
//   );
// }

// auto graphql_selection_set() {
//   return (
//     comb::chr('{').discard() +
//     comb::blankstr(0).discard() +
//     graphql_selection().atleast(0) +
//     comb::blankstr(0).discard() +
//     comb::chr('}').discard()
//   );
// }

// auto graphql_operation_definition() {
//   return (
//     graphql_operation_type() +
//     (comb::blankstr(1) + graphql_name()).optional() +
//     // (comb::blankstr(1) + graphql_variable_definitions()).optional() +
//     // (comb::blankstr(1) + graphql_directives()).optional() +
//     (comb::blankstr(1) + graphql_selection_set())
//   ) || (
//     graphql_selection_set()
//   );
// }

// auto graphql_query() {
//   return (
//     comb::chr('{') &&
//     comb::blankstr(1) &&
//     comb::
//     comb::chr('}')
//   );
// }

auto parse_grapgql_query(std::string_view source) {
  auto parser = (
    graphql_arguments()
  );

  return comb::parse(source, parser);
}

void graphql_test() {
  for (auto& arg : parse_grapgql_query("(width: 200, height: 100)")) {
    std::cout << "name: " << arg.name << " value: " << arg.value << std::endl;
  }
}
