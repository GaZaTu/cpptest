#pragma once

#include "./connection.hpp"
#include "./orm.hpp"

namespace db::orm {
namespace detail {
template <auto Start, auto End, auto Inc, class F>
constexpr void constexpr_for(F&& f) {
  if constexpr (Start < End) {
    constexpr auto i = std::integral_constant<decltype(Start), Start>();

    if (!f(i)) {
      return;
    }

    constexpr_for<Start + Inc, End, Inc>(f);
  }
}
} // namespace detail

class repository {
public:
  repository(db::connection& conn) : _conn{&conn, [](auto) {}} {
  }

  repository(db::datasource& dsrc) : _conn(std::make_shared<db::connection>(dsrc)) {
  }

  template <typename T>
  std::optional<T> findOneById(const typename db::orm::primary<T>::tuple_type& id) {
    using meta = db::orm::meta<T>;
    using primary = db::orm::primary<T>;

    db::orm::selector<T> builder{*_conn};
    builder.select();

    constexpr size_t Start = 0;
    constexpr size_t End = std::tuple_size_v<typename primary::tuple_type>;
    constexpr size_t Inc = 1;

    db::orm::detail::constexpr_for<Start, End, Inc>([&builder, &id](auto I) {
      using field_type = std::tuple_element_t<I, typename primary::tuple_type>;

      constexpr auto id_field_name = primary::class_members[I].name;
      constexpr auto id_field = db::orm::field<field_type>{id_field_name};

      builder.where(id_field == std::get<I>(id));

      return true;
    });

    return builder
      .findOne();
  }

  // template <typename T>
  // std::optional<T> findOneById(const std::optional<typename db::orm::primary<T>::tuple_type>& id) {
  //   if (!id) {
  //     return {};
  //   }

  //   return findOneById<T>(*id);
  // }

  template <typename T>
  int save(T& source) {
    using meta = db::orm::meta<T>;
    using primary = db::orm::primary<T>;

    if constexpr (primary::specialized) {
      constexpr auto primary_len = sizeof(primary::class_members) / sizeof(db::orm::field_info);

      if constexpr (primary_len == 1) {
        auto tuple = primary::tie(source);

        using id_type = std::tuple_element_t<0, typename primary::tuple_type>;
        using idmeta = db::orm::idmeta<id_type>;

        if constexpr (idmeta::specialized) {
          auto& id = std::get<0>(tuple);

          if (id == idmeta::null()) {
            id = idmeta::generate();
          }
        }
      }
    }

    db::orm::inserter<T> builder{*_conn};

    std::vector<std::string> conflict_fields;
    if constexpr (primary::specialized) {
      for (const auto& field : primary::class_members) {
        conflict_fields.push_back(std::string{field.name});
      }
    }

    builder.onConflict(conflict_fields).doReplace();

    for (const auto& field : meta::class_members) {
      builder.set(db::orm::field<void*>{field.name} = nullptr);
    }

    db::statement statement(*_conn);
    builder.prepare(statement);

    int i = 666;
    meta::serialize(statement, source, i);

    return statement.executeUpdate();
  }

  template <typename T>
  int save(std::optional<T>& source) {
    if (source) {
      return save<T>(*source);
    }

    return 0;
  }

  template <typename T>
  int remove(T& source) {
    using meta = db::orm::meta<T>;
    using primary = db::orm::primary<T>;

    auto id = primary::tie(source);

    db::orm::deleter<T> builder{*_conn};

    constexpr size_t Start = 0;
    constexpr size_t End = std::tuple_size_v<typename primary::tuple_type>;
    constexpr size_t Inc = 1;

    db::orm::detail::constexpr_for<Start, End, Inc>([&builder, &id](auto I) {
      using field_type = std::tuple_element_t<I, typename primary::tuple_type>;

      constexpr auto id_field_name = primary::class_members[I].name;
      constexpr auto id_field = db::orm::field<field_type>{id_field_name};

      builder.where(id_field == std::get<I>(id));

      return true;
    });

    int r = builder
      .executeUpdate();

    source = T{};

    return r;
  }

  template <typename T>
  int remove(std::optional<T>& source) {
    if (source) {
      return remove<T>(*source);
    }

    return 0;
  }

  template <typename T>
  orm::selector<T> select() {
    orm::selector<T> builder{*_conn};
    builder.select();
    return builder;
  }

  template <typename T>
  orm::selector<T> select(const std::vector<std::string>& fields) {
    orm::selector<T> builder{*_conn};
    builder.select(fields);
    return builder;
  }

  template <typename T>
  orm::inserter<T> insert() {
    orm::inserter<T> builder{*_conn};
    return builder;
  }

  template <typename T>
  orm::updater<T> update() {
    orm::updater<T> builder{*_conn};
    return builder;
  }

  template <typename T>
  orm::deleter<T> remove() {
    orm::deleter<T> builder{*_conn};
    return builder;
  }

private:
  std::shared_ptr<db::connection> _conn;
};
} // namespace db::orm
