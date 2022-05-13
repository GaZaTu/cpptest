#pragma once

#include "./common.hpp"
#include "./orm-common.hpp"
#include <functional>
#include <memory>
#include <stdint.h>
#include <string_view>
#include <vector>

namespace db {
class datasource {
public:
  class resultset {
  public:
    virtual ~resultset();

    virtual bool next() = 0;

    virtual inline orm::field_type getValueType(std::string_view name) {
      return orm::field_type::UNKNOWN;
    }

    virtual bool isValueNull(std::string_view name) = 0;

    virtual inline void getValue(std::string_view name, bool& result) {
      int32_t tmp;
      getValue(name, tmp);
      result = (bool)tmp;
    }

    virtual inline void getValue(std::string_view name, int8_t& result) {
      int32_t tmp;
      getValue(name, tmp);
      result = (int8_t)tmp;
    }

    virtual inline void getValue(std::string_view name, uint8_t& result) {
      int32_t tmp;
      getValue(name, tmp);
      result = (uint8_t)tmp;
    }

    virtual inline void getValue(std::string_view name, int16_t& result) {
      int32_t tmp;
      getValue(name, tmp);
      result = (int16_t)tmp;
    }

    virtual inline void getValue(std::string_view name, uint16_t& result) {
      int32_t tmp;
      getValue(name, tmp);
      result = (uint16_t)tmp;
    }

    virtual inline void getValue(std::string_view name, int32_t& result) {
      std::string_view tmp;
      getValue(name, tmp);
      result = std::atoi(tmp.data());
    }

    virtual inline void getValue(std::string_view name, uint32_t& result) {
      int32_t tmp;
      getValue(name, tmp);
      result = (uint32_t)tmp;
    }

    virtual inline void getValue(std::string_view name, int64_t& result) {
      std::string_view tmp;
      getValue(name, tmp);
      result = std::atoll(tmp.data());
    }

    virtual inline void getValue(std::string_view name, uint64_t& result) {
      int64_t tmp;
      getValue(name, tmp);
      result = (uint64_t)tmp;
    }

#ifdef __SIZEOF_INT128__
    virtual inline void getValue(std::string_view name, __uint128_t& result) {
      const uint8_t* bytes;
      size_t length;
      getValue(name, bytes, length, true);
      result = *(__uint128_t*)bytes;
    }
#endif

    virtual inline void getValue(std::string_view name, float& result) {
      double tmp;
      getValue(name, tmp);
      result = (float)tmp;
    }

    virtual inline void getValue(std::string_view name, double& result) {
      std::string_view tmp;
      getValue(name, tmp);
      result = std::atof(tmp.data());
    }

    virtual inline void getValue(std::string_view name, long double& result) {
      const uint8_t* bytes;
      size_t length;
      getValue(name, bytes, length, true);
      result = *(long double*)bytes;
    }

    virtual void getValue(std::string_view name, std::string_view& result) = 0;

    virtual inline void getValue(std::string_view name, std::string& result) {
      std::string_view tmp;
      getValue(name, tmp);
      result = (std::string)tmp;
    }

    virtual inline void getValue(std::string_view name, const uint8_t*& result, size_t& length, bool isnumber = false) {
      throw db::sql_error{"not implemented"};
    }

    virtual inline void getValue(std::string_view name, std::vector<uint8_t>& result) {
      const uint8_t* bytes;
      size_t length;
      getValue(name, bytes, length);
      result = {bytes, bytes + length};
    }

    virtual inline void getValue(std::string_view name, orm::date& result) {
      std::string_view tmp;
      getValue(name, tmp);
      result = (orm::date)tmp;
    }

    virtual inline void getValue(std::string_view name, orm::time& result) {
      std::string_view tmp;
      getValue(name, tmp);
      result = (orm::time)tmp;
    }

    virtual inline void getValue(std::string_view name, orm::timestamp& result) {
      std::string_view tmp;
      getValue(name, tmp);
      result = (orm::timestamp)tmp;
    }

    virtual int columnCount() = 0;

    virtual std::string columnName(int i) = 0;
  };

  class statement {
  public:
    virtual ~statement();

    virtual std::shared_ptr<resultset> execute() = 0;

    virtual int executeUpdate() = 0;

    virtual void setParamToNull(std::string_view name) = 0;

    virtual inline void setParam(std::string_view name, std::nullopt_t) {
      setParamToNull(name);
    }

    virtual inline void setParam(std::string_view name, bool value) {
      setParam(name, (int32_t)value);
    }

    virtual inline void setParam(std::string_view name, int8_t value) {
      setParam(name, (int32_t)value);
    }

    virtual inline void setParam(std::string_view name, uint8_t value) {
      setParam(name, (int32_t)value);
    }

    virtual inline void setParam(std::string_view name, int16_t value) {
      setParam(name, (int32_t)value);
    }

    virtual inline void setParam(std::string_view name, uint16_t value) {
      setParam(name, (int32_t)value);
    }

    virtual inline void setParam(std::string_view name, int32_t value) {
      setParam(name, std::to_string(value));
    }

    virtual inline void setParam(std::string_view name, uint32_t value) {
      setParam(name, (int32_t)value);
    }

    virtual inline void setParam(std::string_view name, int64_t value) {
      setParam(name, std::to_string(value));
    }

    virtual inline void setParam(std::string_view name, uint64_t value) {
      setParam(name, (int64_t)value);
    }

#ifdef __SIZEOF_INT128__
    virtual inline void setParam(std::string_view name, __uint128_t value) {
      setParam(name, (const uint8_t*)&value, sizeof(__uint128_t), true);
    }
#endif

    virtual inline void setParam(std::string_view name, float value) {
      setParam(name, (double)value);
    }

    virtual inline void setParam(std::string_view name, double value) {
      setParam(name, std::to_string(value));
    }

    virtual inline void setParam(std::string_view name, long double value) {
      setParam(name, (const uint8_t*)&value, sizeof(long double), true);
    }

    virtual void setParam(std::string_view name, std::string_view value) = 0;

    virtual inline void setParam(std::string_view name, std::string&& value) {
      setParam(name, (std::string_view)value);
    }

    virtual inline void setParam(std::string_view name, const uint8_t* value, size_t length, bool isnumber = false) {
      throw db::sql_error{"not implemented"};
    }

    virtual inline void setParam(std::string_view name, const std::vector<uint8_t>& value) {
      setParam(name, value.data(), value.size());
    }

    virtual inline void setParam(std::string_view name, orm::date value) {
      setParam(name, (std::string)value);
    }

    virtual inline void setParam(std::string_view name, orm::time value) {
      setParam(name, (std::string)value);
    }

    virtual inline void setParam(std::string_view name, orm::timestamp value) {
      setParam(name, (std::string)value);
    }

    virtual inline bool readonly() {
      return false;
    }
  };

  class connection {
  public:
    virtual ~connection();

    virtual std::shared_ptr<statement> prepareStatement(std::string_view script) = 0;

    virtual inline void beginTransaction() {
      throw db::sql_error{"not implemented"};
    }

    virtual inline void commit() {
      throw db::sql_error{"not implemented"};
    }

    virtual inline void rollback() {
      throw db::sql_error{"not implemented"};
    }

    virtual inline bool readonlyTransaction() {
      return false;
    }

    virtual inline void execute(std::string_view script) {
      prepareStatement(script)->execute();
    }

    virtual inline int getVersion() {
      throw db::sql_error{"not implemented"};
    }

    virtual inline void setVersion(int version) {
      throw db::sql_error{"not implemented"};
    }

    virtual inline bool supportsORM() {
      return false;
    }

    virtual inline std::string createInsertScript(const orm::query_builder_data& data, int& param) {
      throw db::sql_error{"not implemented"};
    }

    virtual inline std::string createUpdateScript(const orm::query_builder_data& data, int& param) {
      throw db::sql_error{"not implemented"};
    }

    virtual inline std::string createUpsertScript(const orm::query_builder_data& data, int& param) {
      throw db::sql_error{"not implemented"};
    }

    virtual inline std::string createSelectScript(const orm::query_builder_data& data, int& param) {
      throw db::sql_error{"not implemented"};
    }

    virtual inline std::string createDeleteScript(const orm::query_builder_data& data, int& param) {
      throw db::sql_error{"not implemented"};
    }

    virtual inline std::function<void(std::string_view)>& onPrepareStatement() {
      return _onPrepareStatement;
    }

    virtual inline bool readonly() {
      return false;
    }

    virtual inline int64_t changes() {
      return -1;
    }

  private:
    std::function<void(std::string_view)> _onPrepareStatement;
  };

  virtual ~datasource();

  virtual std::string_view driver() = 0;

  virtual std::shared_ptr<connection> getConnection() = 0;

  virtual inline std::function<void(db::connection&)>& onConnectionOpen() {
    return _onConnectionOpen;
  }

  virtual inline std::function<void(db::connection&)>& onConnectionClose() {
    return _onConnectionClose;
  }

  virtual inline std::vector<orm::update>& updates() {
    return _updates;
  }

private:
  std::function<void(db::connection&)> _onConnectionOpen;
  std::function<void(db::connection&)> _onConnectionClose;

  std::vector<orm::update> _updates;
};
} // namespace db
