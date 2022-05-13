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
  repository(db::connection& conn);

  repository(db::datasource& dsrc);

  template <typename T>
  uint64_t count() {
    using meta = db::orm::meta<T>;
    using primary = db::orm::primary<T>;

    db::orm::selector builder{*_conn};
    builder.select({"count(*)"}).from<T>();

    return builder
      .findOne()
      .firstValue<uint64_t>()
      .value_or(0);
  }

  template <typename T, typename L, condition_operator O, typename R>
  uint64_t count(db::orm::condition<L, O, R>&& condition) {
    using meta = db::orm::meta<T>;
    using primary = db::orm::primary<T>;

    db::orm::selector builder{*_conn};
    builder.select({"count(*)"}).from<T>().where(std::move(condition));

    return builder
      .findOne()
      .firstValue<uint64_t>()
      .value_or(0);
  }

  template <typename T, typename L, condition_operator O, typename R>
  std::optional<typename db::orm::primary<T>::tuple_type> findId(db::orm::condition<L, O, R>&& condition) {
    using meta = db::orm::meta<T>;
    using primary = db::orm::primary<T>;

    db::orm::selector builder{*_conn};
    builder.select().from<T>().where(std::move(condition));

    auto result = builder
      .findOne<T>();

    if (result) {
      return primary::tie(result);
    }

    return std::nullopt;
  }

  template <typename T, typename L, condition_operator O, typename R>
  selection_iterable<T> findMany(db::orm::condition<L, O, R>&& condition) {
    using meta = db::orm::meta<T>;
    using primary = db::orm::primary<T>;

    db::orm::selector builder{*_conn};
    builder.select().from<T>().where(std::move(condition));

    return builder
      .findMany<T>();
  }

  template <typename T>
  selection_iterable<T> findManyById(const std::set<std::tuple_element_t<0, typename db::orm::primary<T>::tuple_type>>& ids) {
    using meta = db::orm::meta<T>;
    using primary = db::orm::primary<T>;

    db::orm::selector builder{*_conn};
    builder.select().from<T>();

    using field_type = std::tuple_element_t<0, typename primary::tuple_type>;

    constexpr auto id_field_name = primary::class_members[0].name;
    constexpr auto id_field = db::orm::field<field_type>{id_field_name};

    builder.where(id_field.IN(ids));

    return builder
      .findMany<T>();
  }

  template <typename T, typename L, condition_operator O, typename R>
  std::optional<T> findOne(db::orm::condition<L, O, R>&& condition) {
    using meta = db::orm::meta<T>;
    using primary = db::orm::primary<T>;

    db::orm::selector builder{*_conn};
    builder.select().from<T>().where(std::move(condition));

    return builder
      .findOne<T>();
  }

  template <typename T>
  std::optional<T> findOneById(const typename db::orm::primary<T>::tuple_type& id) {
    using meta = db::orm::meta<T>;
    using primary = db::orm::primary<T>;

    db::orm::selector builder{*_conn};
    builder.select().from<T>();

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
      .findOne<T>();
  }

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

    // if (!_statements_save.count(meta::class_name)) {

    //   _statements_save.emplace(meta::class_name, std::move(statement));
    // }

    // db::statement& statement = _statements_save.at(meta::class_name);

    db::orm::inserter builder{*_conn};
    builder.into<T>();

    std::vector<std::string> conflict_fields;
    if constexpr (primary::specialized) {
      for (const auto& field : primary::class_members) {
        conflict_fields.push_back(std::string{field.name});
      }
    }

    builder.onConflict(conflict_fields).doReplace();

    auto changed_fields = meta::changes(source);
    for (auto fname : changed_fields) {
      builder.set(db::orm::field<std::optional<bool>>{fname.data()} = std::nullopt);
    }

    db::statement statement{*_conn};
    builder.prepare(statement);

    int i = 666;
    meta::serialize(statement, source, i, changed_fields);

    int changes = statement.executeUpdate();
    if (changes != 0) {
      auto id = primary::tie(source);
      source = findOneById<T>(id).value();
    }

    return changes;
  }

  template <typename T>
  int save(std::optional<T>& source) {
    if (source) {
      return save<T>(*source);
    }

    return 0;
  }

  template <typename T>
  int save(std::vector<T>& source) {
    int c = 0;

    for (T& item : source) {
      c += save<T>(item);
    }

    return c;
  }

  template <typename T>
  int remove(T& source) {
    using meta = db::orm::meta<T>;
    using primary = db::orm::primary<T>;

    auto id = primary::tie(source);

    db::orm::deleter builder{*_conn};
    builder.from<T>();

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
  int remove(std::vector<T>& source) {
    int c = 0;

    for (T& item : source) {
      c += remove<T>(item);
    }

    return c;
  }

  db::orm::selector select();

  db::orm::selector select(const std::vector<db::orm::selection>& fields);

  db::orm::inserter insert();

  db::orm::updater update();

  db::orm::deleter remove();

  inline operator db::connection&() {
    return *_conn;
  }

private:
  std::shared_ptr<db::connection> _conn;

  // std::unordered_map<std::string_view, db::statement> _statements_save;
  // std::unordered_map<std::string_view, db::statement> _statements_remove;
};

// template <typename T>
// struct managed : public std::shared_ptr<T> {
// public:
//   managed(db::orm::repository& repo) : std::shared_ptr<T>(), _repo(repo) {}

//   ~managed() {
//     if (!this) {
//       return;
//     }

//     if constexpr (!std::is_const_v<T>) {
//       _repo.save(*this)
//     }
//   }

// private:
//   db::orm::repository& _repo;
// };
} // namespace db::orm
