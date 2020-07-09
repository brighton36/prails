#pragma once
#include <string>
#include <sstream> 
#include <variant>
#include <optional>
#include <functional>
#include <experimental/type_traits>

#include "spdlog/spdlog.h"

#include <soci/soci.h>
#include <soci/sqlite3/soci-sqlite3.h>
#include <soci/mysql/soci-mysql.h>

#include "exceptions.hpp"
#include "config_parser.hpp"
#include "functions.hpp"

#define MODEL_CONSTRUCTOR(classname) \
  explicit classname() : Instance(&classname::Definition) {}; \
  explicit classname(const Model::Record &r, bool isDirty = true) : \
    Instance(r, isDirty, &classname::Definition) {};

// I suppose we could use a template here, and do without this macro...:
// https://stackoverflow.com/questions/9065081/how-do-i-get-the-argument-types-of-a-function-pointer-in-a-variadic-template-cla
#define MODEL_ACCESSOR(name, type) \
  std::optional<type> name() { \
    std::optional<Model::RecordValue> val = recordGet(#name); \
    return (val) ? std::optional<type>(std::get<type>(*val)) : std::nullopt; \
  }; \
  void name(const std::optional<Model::RecordValue> &val) { recordSet(#name, val); }; \

#define COL_TYPE(type) variant_index<Model::RecordValue, type>()

template<typename VariantType, typename T, std::size_t index = 0>
constexpr std::size_t variant_index() {
  if constexpr (index == std::variant_size_v<VariantType>) {
    return index;
  } else if constexpr (std::is_same_v<std::variant_alternative_t<index, VariantType>, T>) {
    return index;
  } else {
    return variant_index<VariantType, T, index + 1>();
  }
} 

class SpdoutStringbuf : public std::stringbuf {
  public:
    virtual std::streamsize xsputn( const char_type* s, std::streamsize count ) {
      if (count > 1) spdlog::debug("DB Query: {}", std::string(s));
      return count;
    }
};

namespace Model {
  class Definition;

  typedef std::map<std::string, std::size_t> ColumnTypes;
  typedef std::variant<std::string,std::tm,double,int,unsigned long,long> RecordValue;
  typedef std::map<std::string, std::optional<RecordValue>> Record;
  typedef std::optional<std::pair<std::optional<std::string>, std::string>> RecordError;
  typedef std::map<std::optional<std::string>, std::vector<std::string>> RecordErrors;
  typedef std::pair<std::string,Record> Conditional;
  typedef std::function<std::optional<Conditional>(Model::Record &)> AddConditionals;

  struct Validator {
    template <class T>
    Validator(T t) : 
      isValid([t = std::move(t)](Model::Record &r, const Model::Definition &d) { 
        return t.isValid(r,d); }) {}

    std::function<RecordError(Model::Record &r, const Model::Definition &)> isValid;
  };
  typedef std::vector<Validator> Validations;


  // NOTE: This is a bit ... unpolished. The TLDR here is that namespace-local
  // variables are thread-local. So, it does us no good to store these static 
  // variables in the namespace. They're really global variables. But, I wanted
  // to keep this library header-only, so, the compromise is to stick these
  // static variables inside a function. Where, they are guaranteed to persist
  // globally. The syntax of this is slightly odd right now, because it expects
  // a single instantiation, followed by calls without parameters.
  //
  // It's probable that in a more finished version, we would want to store multiple
  // pools, to support multiple simultaneous database connections. But, I have 
  // no need for that now, so, this is the result. we're keep the initialize()
  // interface just for the clarity of intent in the rest of the codebase.
  soci::session inline GetSession(unsigned int threads = 0, std::string dsn = "") { 
    static std::shared_ptr<soci::connection_pool> default_pool = nullptr;
    static SpdoutStringbuf debug_buff;
    static std::ostream spd_debug_out(&debug_buff);

    if (default_pool == nullptr) {
      if (threads == 0 || dsn.empty())
        throw ModelException(
        "Unable to retrieve a database session. "
        "The database connection has not yet been initialized.");

      default_pool = std::make_shared<soci::connection_pool>(threads);

      for (unsigned int i = 0; i != threads; ++i) {
        soci::session & sql = default_pool->at(i);
        sql.set_log_stream(&spd_debug_out); // TODO: Not thread-safe, I think
        sql.open(dsn);
      }
    } else if ((threads != 0) || (!dsn.empty()))
        throw ModelException(
        "Unable to re-initialize database. The Database pool is already established");

    return soci::session(*default_pool); 
  }

  std::tm inline NowUTC() {
    time_t t_time = time(NULL);
    return *gmtime(&t_time);
  }

  // NOTE: See the GetSession() notes above.
  void inline Initialize(ConfigParser &config) {
    GetSession(config.threads(), config.dsn());
  }

  class Definition {
    public:
      explicit Definition(const std::string &pkey_column, 
        const std::string &table_name, const ColumnTypes &column_types, 
        const Validations &validations) : 
        pkey_column(pkey_column), table_name(table_name), 
        column_types(column_types), validations(validations) {

        // Check that pkey exists, and is long:
        if ((column_types.find(pkey_column) == column_types.end()) || 
          (column_types.at(pkey_column) != COL_TYPE(long)))
          throw ModelException(
          "Invalid model definition. References pkey \"{}\", which cant be "
          "found in ColumnTypes, or which isn't of type long.", pkey_column);
      };

      std::string pkey_column; // NOTE: This column must be a long as of this time.
      std::string table_name;
      ColumnTypes column_types;
      Validations validations;
  };

  template <class T>
  class Instance {
    protected:
      explicit Instance(const Model::Definition* const);
      explicit Instance(Model::Record, bool, const Model::Definition* const);

      static long GetAffectedRows(soci::statement &, soci::session &);

      bool isDirty_;
      Model::RecordErrors errors_;
      std::optional<bool> isValid_;
      const Model::Definition* definition;
      std::shared_ptr<soci::connection_pool> pool;
      Model::Record record;

    public: 
      bool isValid();
      bool isDirty();
      void save();
      void remove();
      void resetStateCache();
      Model::RecordErrors errors();
      std::optional<Model::RecordValue> recordGet(const std::string&);
      void recordSet(const std::string&, const std::optional<Model::RecordValue>&);
      std::vector<std::string> recordKeys();
      std::vector<std::string> modelKeys();

      static void Remove(long);
      static void Migrate();
      static Model::Record RowToRecord(soci::row &);
      static void Remove(std::string, long);
      static std::optional<T> Find(long);
      static std::optional<T> Find(std::string, Model::Record);
      template <typename... Args> 
      static std::vector<T> Select(std::string, Args...);
      template <typename... Args> 
      static unsigned long Count(std::string, Args...);
      static void CreateTable(std::vector<std::pair<std::string,std::string>>);
      static void DropTable();
  };

  class ColumnValidator {
    public:
      explicit ColumnValidator(const std::string &column) : column(column) {};

      RecordError isValid(Model::Record &, const Model::Definition &) const { 
        return std::nullopt;
      }

    protected:
      std::string column;
      bool hasValue(Model::Record &r) const { 
        return ((r.find(column) != r.end()) && r[column].has_value());
      }
      bool hasValue(Model::Record &r, std::size_t of_type) const {
        if (r.find(column) == r.end()) return false;
        std::optional<RecordValue> val = r[column];
        return ((val) && (val.value().index() == of_type));
      }
      RecordError error(const std::string &message) const {
        return RecordError({column, message}); 
      }
  };


  namespace Validates {

    class NotNull : public ColumnValidator {
      public:
        using ColumnValidator::ColumnValidator;

        const std::string ErrorMessage = "is missing";

        RecordError isValid(Model::Record &r, const Model::Definition &) const {
          if (!hasValue(r)) return error(ErrorMessage);
          return std::nullopt;
        }
    };

    class NotEmpty : public ColumnValidator {
      public:
        using ColumnValidator::ColumnValidator;

        const std::string ErrorMessage = "is empty";

        RecordError isValid(Model::Record &r, const Model::Definition &) const {
          if (!hasValue(r)) return std::nullopt;

          if ( (!hasValue(r, COL_TYPE(std::string))) || 
            (std::get<std::string>(*r[column]).empty() ) 
          ) return error(ErrorMessage);
          
          return std::nullopt;
        }
    };

    class IsBoolean : public ColumnValidator {
      public:
        using ColumnValidator::ColumnValidator;

        const std::string ErrorMessage = "isnt a yes or no value";

        RecordError isValid(Model::Record &r, const Model::Definition &) const {
          if (!hasValue(r)) return std::nullopt;

          if ( (!hasValue(r, COL_TYPE(int))) || 
            (std::get<int>(*r[column]) > 1 ) || (std::get<int>(*r[column]) < 0)
          ) return error(ErrorMessage);

          return std::nullopt;
        }
    };

    class Matches : public ColumnValidator {
      private:
        std::regex against;
      public:
        const std::string ErrorMessage = "doesn't match the expected format";

        explicit Matches(const std::string &column, std::regex against) : 
          ColumnValidator(column), against(against) {};

        RecordError isValid(Model::Record &r, const Model::Definition &) const {
          if (!hasValue(r)) return std::nullopt;

          if ( (!hasValue(r, COL_TYPE(std::string))) || 
            (!std::regex_match(std::get<std::string>(*r[column]), against)) 
          ) return error(ErrorMessage);

          return std::nullopt;
        }
    };

    // TODO: We should write some tests here...
    class MaxLength : public ColumnValidator {
      private:
        unsigned int max_length;
      public:
        const std::string ErrorMessage = "has too many characters. The maximum length is {}.";

        explicit MaxLength(const std::string &column, unsigned int max_length) : 
          ColumnValidator(column), max_length(max_length) {};

        RecordError isValid(Model::Record &r, const Model::Definition &) const {
          if (!hasValue(r)) return std::nullopt;

          if ( (!hasValue(r, COL_TYPE(std::string))) || 
            (std::get<std::string>(*r[column]).length() > max_length) 
          ) return error(fmt::format(ErrorMessage, max_length));

          return std::nullopt;
        }
    };

    class IsUnique : public ColumnValidator {
      private:
        AddConditionals add_conditionals;
      public:
        using ColumnValidator::ColumnValidator;

        const std::string ErrorMessage = "has already been registered";

        explicit IsUnique(const std::string &column) : ColumnValidator(column) {};
        explicit IsUnique(const std::string &column, 
          const AddConditionals &add_conditionals) : 
          ColumnValidator(column), add_conditionals(add_conditionals) {};

        RecordError isValid(Model::Record &record, 
          const Model::Definition &definition) const {

          if (!hasValue(record)) return std::nullopt;

          soci::session sql = Model::GetSession();
          soci::statement st(sql);

          // Put the query together:
          std::string query = "select count(*) from "+definition.table_name;

          Record where;
          where[column] = record[column];
          query += " where "+column+" = :"+column;

          if (record[definition.pkey_column]) {
            where[definition.pkey_column] = record[definition.pkey_column];
            query += " and "+definition.pkey_column+" != :"+definition.pkey_column;
          } 

          if (add_conditionals) {
            auto conditionals = add_conditionals(record);
            if (conditionals) {
              query += " and "+(*conditionals).first;
              for (const auto &p: (*conditionals).second) where[p.first] = p.second;
            }
          }

          st.exchange(soci::use(&where));

          long found_records;
          st.exchange(soci::into(found_records));
          st.alloc();
          st.prepare(query);
          st.define_and_bind();

          if (!st.execute(true)) 
            throw ModelException("No data returned for count query");

          return (found_records > 0) ? error(ErrorMessage) : std::nullopt;
        }
    };
  }
}

namespace soci {
  
  template<>
  struct type_conversion<Model::Record *> {
    typedef values base_type;
    static void from_base(values const &, indicator, Model::Record *) {}
    static void to_base(Model::Record * record,values & v, indicator & ind) {
      for (const auto &p: *record) {
        std::string key = p.first;
        std::optional<Model::RecordValue> record_val = p.second;

        if (record_val.has_value())
          std::visit([&key, &v](auto&& typeA) {
            using U = std::decay_t<decltype(typeA)>;
            v.set<U>(key, typeA, i_ok);
          }, record_val.value());
        else
          v.set(key, 0, i_null);
      }

      ind = i_ok;
    }
  };

  template<>
  struct type_conversion<std::optional<Model::RecordValue> *> {
    typedef values base_type;
    static void from_base(values const &, indicator, 
      std::optional<Model::RecordValue> *) {}
    static void to_base(std::optional<Model::RecordValue> * record_val, 
      values & v, indicator & ind) {
      if (record_val->has_value())
        std::visit([&v](auto&& typeA) {
          using U = std::decay_t<decltype(typeA)>;
          v.set<U>(typeA, i_ok);
        }, record_val->value());
      else
        v.set(0, i_null);

      ind = i_ok;
    }
  };

  template<typename T>
  struct type_conversion<Model::Instance<T>*> {
    typedef values base_type;

    static void from_base(values const &, indicator, Model::Instance<T> *) {}
    static void to_base(Model::Instance<T> * m, values & v, indicator & ind) {
      for (const std::string & key: m->modelKeys() ) {
        auto record_val = m->recordGet(key);

        if (record_val)
          std::visit([&key, &record_val, &v](auto&& typeA) {
            using U = std::decay_t<decltype(typeA)>;
            v.set(key, std::get<U>(record_val.value()), i_ok);
          }, (*record_val));
        else
          v.set(key, 0, i_null);
      }

      ind = i_ok;
    }
  };
}

template <class T>
Model::Instance<T>::Instance(const Model::Definition * const definition) : 
  isDirty_(true), isValid_(std::nullopt), definition(definition) {}

template <class T>
Model::Instance<T>::Instance(Model::Record record_, bool isDirty, 
  const Model::Definition * const definition) : isValid_(std::nullopt), 
  definition(definition) {

  // This mostly enforces type checks:
  for (const auto &col_val : record_) recordSet(col_val.first, col_val.second);
  isDirty_ = isDirty;
}

// This returns all the keys we have:
template <class T>
std::vector<std::string> Model::Instance<T>::recordKeys() {
  std::vector<std::string> ret;  
  std::for_each(record.begin(), record.end(), [&ret](auto &col_val) {
    ret.push_back(col_val.first); });

  return ret;
}

// This gives us the record keys that are persistable:
template <class T>
std::vector<std::string> Model::Instance<T>::modelKeys() {
  std::vector<std::string> ret;  
  for (const auto &col_val : record) 
    if (definition->column_types.count(col_val.first) > 0) 
      ret.push_back(col_val.first);

  return ret;
}

template <class T>
bool Model::Instance<T>::isDirty() { return isDirty_; }

template <class T>
void Model::Instance<T>::resetStateCache() { 
  isDirty_ = true; 
  isValid_ = std::nullopt; 
}

template <class T>
bool Model::Instance<T>::isValid() {
  if (isValid_.has_value()) return isValid_.value();
  isValid_ = (errors().size() == 0);
  return isValid_.value();
}

template <class T>
Model::RecordErrors Model::Instance<T>::errors() {
  // This lets us cache this routine between calls. The typical isValid()... save()
  // sequence would otherwise have us running this (sometimes expensive) multiple
  // times in the sequence.
  if (isValid_.has_value()) return errors_;

  // Rebuild this cache:
  errors_.clear();

  for (auto &validator : definition->validations) {
    Model::RecordError error = validator.isValid(record, *definition);
    if (error) { 
      std::optional<std::string> column = (*error).first;
      std::string message = (*error).second;

      if (errors_.find(column) == errors_.end())
        errors_[column] = std::vector<std::string>();

      errors_[column].push_back(message);
    } 
  }

  return errors_;
}

template <class T>
std::optional<Model::RecordValue> Model::Instance<T>::recordGet(const std::string &col) {
  return (record.count(col) > 0) ? record[col] : std::nullopt;
}

template <class T>
void Model::Instance<T>::save() {
  if (!isDirty()) return;

  if (!isValid()) throw ModelException("Invalid Model can't be saved.");

  soci::session sql = Model::GetSession();

  std::vector<std::string> columns = modelKeys();

  if (recordGet(definition->pkey_column)) {
    std::vector<std::string> set_pairs;
    for (auto &key : columns) 
      if (key != definition->pkey_column) set_pairs.push_back(key+" = :"+key);

    soci::statement update = (sql.prepare << fmt::format("update {} set {} where id = :id", 
      definition->table_name, join(set_pairs, ", ") ), soci::use(this));
    update.execute(true);

    // See the below note on last_insert_id. Seems like affected_rows is similarly
    // off.
    if (long affected_rows = GetAffectedRows(update, sql); affected_rows != 1)
      throw ModelException("Unable to perform update, {} affected rows.", affected_rows);

  } else {
    std::vector<std::string> values;
    std::for_each(columns.begin(), columns.end(), [&values](auto &key) {
      values.push_back(":"+key); });

    sql << fmt::format("insert into {} ({}) values({})", definition->table_name, 
      join(columns, ", "), join(values, ", ")), soci::use(this);

    // NOTE: There appears to be a bug in the pooled session code of soci, that 
    // causes weird typecasting issues from the long long return value of 
    // sqlite3_last_insert_row_id. So, here, we just grab the connection manually
    // and run the typecast. This isn't that portable. so, perhaps we'll fix that
    // at some point.
    long last_id = 0;

    if (sql.get_backend_name() == "sqlite3") { 
      auto sql3backend = static_cast<soci::sqlite3_session_backend *>(sql.get_backend());

      long long sqlite_last_id = sqlite3_last_insert_rowid(sql3backend->conn_);

      if(!sqlite_last_id)
        throw ModelException("Unable to perform insert, last_insert_id returned zero.");

      last_id = static_cast<long>(sqlite_last_id);
    } else if (sql.get_backend_name() == "mysql") { 
      auto mysqlbackend = static_cast<soci::mysql_session_backend *>(sql.get_backend());

      uint64_t mysql_last_id = mysql_insert_id(mysqlbackend->conn_);

      if(!mysql_last_id)
        throw ModelException("Unable to perform insert, last_insert_id returned zero.");

      last_id = static_cast<long>(mysql_last_id);
    } else if (!sql.get_last_insert_id(definition->table_name, last_id))
      throw ModelException("Unable to perform insert, last_insert_id returned zero.");

    recordSet(definition->pkey_column, last_id);
  }

  isDirty_ = false;
}

template <class T>
void Model::Instance<T>::remove() {
  if (!recordGet(definition->pkey_column))
    throw ModelException("Cannot delete a record that has no id");

  Model::Instance<T>::Remove(definition->table_name,
    std::get<long>(*recordGet(definition->pkey_column)));
}

template <class T>
void Model::Instance<T>::recordSet(const std::string &col, const std::optional<Model::RecordValue> &val) {
  if ((definition->column_types.count(col) > 0) && (val != std::nullopt) && 
    ((*val).index() != definition->column_types.at(col))) {
      Model::RecordValue store_val;

      switch (definition->column_types.at(col)) {
        case COL_TYPE(std::string): store_val = std::string(); break;
        case COL_TYPE(double): store_val = (double) 0; break;
        case COL_TYPE(int): store_val = (int) 0; break;
        case COL_TYPE(unsigned long): store_val = (unsigned long) 0; break;
        case COL_TYPE(long): store_val = (long) 0; break;
        case COL_TYPE(std::tm): store_val = std::tm(); break;
        default:
         throw ModelException("Unable to determine column type of column {}", col);
         break;
      }

      std::visit([&store_val, &col](auto&& fromType, auto&& toType) {
        using V = std::decay_t<decltype(fromType)>;
        using U = std::decay_t<decltype(toType)>;

        if constexpr(std::is_same_v<V, std::tm> || std::is_same_v<U, std::tm>){
          throw ModelException("Time conversions are unsupported on {} column.", col);
        } else if constexpr (!std::is_same_v<V, std::string> && !std::is_same_v<U, std::string>)
          store_val = U(fromType);
        else if constexpr (!std::is_same_v<U, std::string> && !std::is_same_v<V, double>)
          // Seems like sqlite returns Scientific notation in a string:
          store_val = atof(fromType.c_str());
        else
          throw ModelException("Invalid type passed to set operation on {} column. "
            "Probably a numeric to non-numeric type mismatch occurred", col);
      }, (*val), store_val);

      record[col] = store_val;
  } else 
    record[col] = val;

  isDirty_ = true;
  isValid_ = std::nullopt;
}

template <class T>
Model::Record Model::Instance<T>::RowToRecord(soci::row &r) {
  Model::Record ret;

  for(size_t i = 0; i != r.size(); ++i) {
    const soci::column_properties & props = r.get_properties(i);
    const soci::indicator & ind = r.get_indicator(i);

    std::string key = props.get_name();

    if (ind == soci::i_null) 
      ret[key] = std::nullopt;
    else {
      Model::RecordValue val;

      switch(props.get_data_type()) {
        case soci::dt_string: val = r.get<std::string>(i); break;
        case soci::dt_double: val = r.get<double>(i); break;
        case soci::dt_integer: val = r.get<int>(i); break;
        case soci::dt_unsigned_long_long: val = r.get<unsigned long>(i); break;
        case soci::dt_long_long: val = r.get<long>(i); break;
        case soci::dt_date: val = r.get<tm>(i); break;
      }
      ret[key] = val;
    }
  }

  return ret;
}

template <class T>
void Model::Instance<T>::Remove(std::string table_name, long id) {

  soci::session sql = Model::GetSession();
  soci::statement delete_stmt = (sql.prepare << 
    fmt::format("delete from {} where id = :id", table_name), 
    soci::use(id, "id"));
  delete_stmt.execute(true);

  if (long affected_rows = GetAffectedRows(delete_stmt, sql); affected_rows != 1)
    throw ModelException("Error deleting {} record with id {}. {} rows affected.", 
      table_name, id, affected_rows);
}

template <class T>
std::optional<T> Model::Instance<T>::Find(long id){
  return Model::Instance<T>::Find("id = :id", Model::Record({{"id", id}}));
}

template <class T>
std::optional<T> Model::Instance<T>::Find(std::string where, Model::Record where_values){
  soci::session sql = Model::GetSession();
  soci::row r;

  sql << fmt::format("select * from {} where {} limit 1", 
    T::Definition.table_name, where), soci::use(&where_values), soci::into(r);

  if (!sql.got_data()) return std::nullopt;

  return std::make_optional(T(RowToRecord(r), false));
}

template <class T>
void Model::Instance<T>::Remove(long id) {
  Remove(T::Definition.table_name, id);
}

template <class T>
template <typename... Args> 
std::vector<T> Model::Instance<T>::Select(std::string query, Args... args) {
  soci::session sql = Model::GetSession();
  soci::statement st(sql);
  soci::row rows;
  std::vector<T> ret;

  ((void) st.exchange(soci::use<Args>(args)), ...);

  st.alloc();
  st.prepare(query);
  st.define_and_bind();
  st.exchange_for_rowset(soci::into(rows));
  st.execute(false);

  soci::rowset_iterator<soci::row> it(st, rows);
  soci::rowset_iterator<soci::row> end;
  for (; it != end; ++it) ret.push_back(T(RowToRecord(*it), false));

  return ret;
}

template <class T>
template <typename... Args> 
unsigned long Model::Instance<T>::Count(std::string query, Args... args){
  soci::session sql = Model::GetSession();
  long count;

  soci::statement st(sql);

  ((void) st.exchange(soci::use<Args>(args)), ...);

  st.exchange(soci::into(count));
  st.alloc();
  st.prepare(query);
  st.define_and_bind();
  bool got_data = st.execute(true);

  if (!got_data) throw ModelException("No data returned for count query");

  return count;
}

template <class T>
long Model::Instance<T>::GetAffectedRows(soci::statement &statement, soci::session &sql) {
  // NOTE: This may belong in a quasi-driver kind of thing. It's a hack to get 
  //       this long, which varies by backend I guess...

  if (sql.get_backend_name() == "mysql") {
    auto mysqlbackend = static_cast<soci::mysql_session_backend *>(sql.get_backend());

    uint64_t mysql_rows = mysql_affected_rows(mysqlbackend->conn_);

    return static_cast<long>(mysql_rows);
  } else 
    return statement.get_affected_rows();

  return 0;
}

template <class T>
void Model::Instance<T>::CreateTable(std::vector<std::pair<std::string,std::string>> columns) {
  auto sql = Model::GetSession();

  std::string joined_columns;
  for (const auto &column : columns)
    joined_columns.append(", "+column.first+" "+column.second);

  if (sql.get_backend_name() == "sqlite3") {
    sql << fmt::format( 
      "create table if not exists {} ( {} integer primary key {} )", 
      T::Definition.table_name, T::Definition.pkey_column, joined_columns);
  } else if (sql.get_backend_name() == "mysql") { 
    sql << fmt::format( 
      "create table if not exists {} ( {} integer NOT NULL AUTO_INCREMENT {}, PRIMARY KEY({}) )", 
      T::Definition.table_name, T::Definition.pkey_column, joined_columns, 
      T::Definition.pkey_column);
  } else 
    throw ModelException("Unrecognized backend. Unable to create table");
}

template <class T>
void Model::Instance<T>::DropTable() {
  auto sql = Model::GetSession();

  // NOTE: I don't think there's any way to error test this, other than to expect
  // for an error to throw from soci...
  sql << fmt::format("drop table {}", T::Definition.table_name);
}
