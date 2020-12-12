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

#include "model_factory.hpp"
#include "exceptions.hpp"
#include "config_parser.hpp"
#include "utilities.hpp"

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

namespace Model {
  class Definition;

  typedef std::map<std::string, std::size_t> ColumnTypes;
  typedef std::variant<std::string,std::tm,double,int,unsigned long,long> RecordValue;
  typedef std::map<std::string, std::optional<RecordValue>> Record;
  typedef std::optional<std::pair<std::optional<std::string>, std::string>> RecordError;
  typedef std::map<std::optional<std::string>, std::vector<std::string>> RecordErrors;
  typedef std::pair<std::string,Record> Conditional;
  typedef std::function<std::optional<Conditional>(Model::Record)> AddConditionals;

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

  class Validator {
    public:
      template <class T>
      Validator(T t) {
        // If we don't make a local copy of t, things get weird and crashy:
        passed_obj = std::make_shared<T>(t);
        isValid = std::bind(&T::isValid, static_cast<T*>(passed_obj.get()), 
          std::placeholders::_1, std::placeholders::_2);
      }

      // TODO: I think we may not be deconconstructing passed_ob correctly. Test.
      ~Validator() {}

      std::function<RecordError(Model::Record &r, const Model::Definition &)> isValid;
    private:
      std::shared_ptr<ColumnValidator> passed_obj;
  };
  typedef std::vector<Validator> Validations;

	static void Log(const std::string &query) { ModelFactory::Log(query); }

  std::tm inline NowUTC() {
    time_t t_time = time(NULL);

    struct tm ret;
    memset(&ret, 0, sizeof(tm));
    memcpy(&ret, gmtime(&t_time), sizeof(tm));

    return ret;
  }

  class Definition {
    public:
      explicit Definition(const std::string &pkey_column, 
        const std::string &table_name, const ColumnTypes column_types, 
        const Validations validations, bool is_persisting_in_utc = true) : 
        pkey_column(pkey_column), table_name(table_name), 
        column_types(column_types), validations(validations), 
        is_persisting_in_utc(is_persisting_in_utc) {

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
      bool is_persisting_in_utc = true;
  };

  template <class T>
  class Instance {
    protected:
      static long GetAffectedRows(soci::statement &, soci::session &);

      bool isDirty_;
      Model::RecordErrors errors_;
      std::optional<bool> isValid_;
      const Model::Definition* definition;
      std::shared_ptr<soci::connection_pool> pool;
      Model::Record record;

    public: 
      Instance();
      Instance(Model::Record);
      Instance(Model::Record, bool);
      Instance(const Model::Definition* const);
      Instance(Model::Record, bool, const Model::Definition* const);

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
        const std::string ErrorMessage = "has too many characters. The maximum length is {max_length}.";

        explicit MaxLength(const std::string &column, unsigned int max_length) : 
          ColumnValidator(column), max_length(max_length) {};

        RecordError isValid(Model::Record &r, const Model::Definition &) const {
          if (!hasValue(r)) return std::nullopt;

          if ( (!hasValue(r, COL_TYPE(std::string))) || 
            (std::get<std::string>(*r[column]).length() > max_length) 
          ) return error(fmt::format(ErrorMessage, fmt::arg("max_length", max_length)));

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
          const AddConditionals add_conditionals) : 
          ColumnValidator(column), add_conditionals(add_conditionals) {};

        RecordError isValid(Model::Record &record, 
          const Model::Definition &definition) const {

          if (!hasValue(record)) return std::nullopt;

          soci::session sql = ModelFactory::getSession("default");
          soci::statement st(sql);

          // Put the query together:
          std::string query = "select count(*) from "+definition.table_name;


          // NOTE: For reasons I don't understand, it seems we need to allocate
          // and pass this as a pointer. A reference to a local object segfaults...
          // I think this has to do with how these validators are initialized
          // in the objects... Weirdly, make_shared gives us a bad_alloc ...
          auto where = new Record();
          if(record[column].has_value()) {
            where->insert({column, record[column]});
            query += " where "+column+" = :"+column;
          }

          if (record[definition.pkey_column]) {
            where->insert({definition.pkey_column, record[definition.pkey_column]});
            query += " and "+definition.pkey_column+" != :"+definition.pkey_column;
          } 

          if (add_conditionals) {
            auto conditionals = add_conditionals(record);
            if (conditionals) {
              query += " and "+(*conditionals).first;
              for (const auto &p: (*conditionals).second)
                where->insert_or_assign(p.first, p.second);
            }
          }

					Model::Log(query);

          st.exchange(soci::use(where));

          long found_records;
          st.exchange(soci::into(found_records));
          st.alloc();
          st.prepare(query);
          st.define_and_bind();

          auto execute_ret = st.execute(true);
          delete where;

          if (!execute_ret) 
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
Model::Instance<T>::Instance() : Model::Instance<T>::Instance(&T::Definition) {}

template <class T>
Model::Instance<T>::Instance(Model::Record record_) : 
  Model::Instance<T>::Instance(record_, true, &T::Definition) {}

template <class T>
Model::Instance<T>::Instance(Model::Record record_, bool isDirty) : 
  Model::Instance<T>::Instance(record_, isDirty, &T::Definition) {}

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
  if ( (record.count(col) == 0) || (!record[col].has_value()) )
    return std::nullopt;

  // tm's are a special case, where the persisted value, may need to be adjusted
  // to either utc or local:
  if ((*record[col]).index() == COL_TYPE(std::tm)) {
    std::tm provided_tm = std::get<std::tm>(*record[col]);

    if (!definition->is_persisting_in_utc) {
      time_t provided_t = timelocal(&provided_tm);
      memcpy(&provided_tm, localtime(&provided_t), sizeof(tm));
    }

    return std::make_optional<Model::RecordValue>(provided_tm);
  }

  return record[col];
}

template <class T>
void Model::Instance<T>::save() {
  if (!isDirty()) return;

  if (!isValid()) throw ModelException("Invalid Model can't be saved.");

  soci::session sql = ModelFactory::getSession("default");

  std::vector<std::string> columns = modelKeys();

  // TODO: We may have a case of a new model, with a set id ...
  if (recordGet(definition->pkey_column)) {
    std::vector<std::string> set_pairs;
    for (auto &key : columns) 
      if (key != definition->pkey_column) set_pairs.push_back(key+" = :"+key);

    soci::statement update = (sql.prepare << fmt::format(
      "update {table_name} set {update_pairs} where id = :id", 
      fmt::arg("table_name", definition->table_name),
      fmt::arg("update_pairs", prails::utilities::join(set_pairs, ", "))
      ), soci::use(this));
    update.execute(true);

    // See the below note on last_insert_id. Seems like affected_rows is similarly
    // off.
    if (long affected_rows = GetAffectedRows(update, sql); affected_rows != 1)
      throw ModelException("Unable to perform update, {} affected rows.", affected_rows);

  } else {
    std::vector<std::string> values;
    std::for_each(columns.begin(), columns.end(), [&values](auto &key) {
      values.push_back(":"+key); });

    std::string query = fmt::format(
      "insert into {table_name} ({columns}) values({values})", 
      fmt::arg("table_name", definition->table_name),
      fmt::arg("columns", prails::utilities::join(columns, ", ")),
      fmt::arg("values", prails::utilities::join(values, ", ")));

		Model::Log(query);

		sql << query, soci::use(this);

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
  // First we'll mark the record state:
  isDirty_ = true;
  isValid_ = std::nullopt;

  // If they're setting the value to null, this path is easy:
  if (val == std::nullopt) {
    record[col] = std::nullopt;
    return;
  }

  // Is there a type enforcement involved?
  Model::RecordValue store_val = *val;

  if (definition->column_types.count(col) > 0) {
    // This path means there's a conversion of val, into the COL_TYPE:
    if ((*val).index() != definition->column_types.at(col)) {
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

				// cppcheck takes umbrage with this valid if statement. Disable that:
				// cppcheck-suppress internalAstError
        if constexpr (std::is_same_v<V, std::tm> || std::is_same_v<U, std::tm>) {
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

    } else if (definition->column_types.at(col) == COL_TYPE(std::tm)) {
      // For the case of a tm, there may have to be adjustments to the time, 
      // depending on what tm_gmtoff was provided, and what we're storing.
      // NOTE: This may not actually be what people expect, if they pass us
      // a tm that's neither a local zone, or a utc zone. 

      std::tm store_val_tm = std::get<std::tm>(*val);

      // If we were presented a non-utc time, interpret it as local:
      time_t provided_t = (store_val_tm.tm_gmtoff != 0) ? 
        timelocal(&store_val_tm) : timegm(&store_val_tm);

      memcpy(&store_val_tm, 
        (definition->is_persisting_in_utc) ? gmtime(&provided_t) : localtime(&provided_t), 
        sizeof(tm));

      store_val = store_val_tm;
    }

  } 

  record[col] = store_val;
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
        case soci::dt_date:
          // NOTE: Soci provides us local tm's. Here, we're going to strip all
          // zone information, and return the tm with the provided datetime, 
          // but with the zone set to UTC.

          std::tm tm_from_soci = r.get<std::tm>(i);
          time_t t_from_soci;
          std::tm ret;
          if (T::Definition.is_persisting_in_utc) {
            t_from_soci = timegm(&tm_from_soci);
            memcpy(&ret, gmtime(&t_from_soci), sizeof(tm));
          } else {
            t_from_soci = mktime(&tm_from_soci);
            memcpy(&ret, localtime(&t_from_soci), sizeof(tm));
          }
          val = ret; 
          break;
      }
      ret[key] = val;
    }
  }

  return ret;
}

template <class T>
void Model::Instance<T>::Remove(std::string table_name, long id) {

  soci::session sql = ModelFactory::getSession("default");
	std::string query = fmt::format("delete from {table_name} where id = :id", 
    fmt::arg("table_name", table_name));

	Model::Log(query);

  soci::statement delete_stmt = (sql.prepare << query, soci::use(id, "id"));
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
  soci::session sql = ModelFactory::getSession("default");
  soci::row r;

	std::string query = fmt::format(
    "select * from {table_name} where {where} limit 1", 
    fmt::arg("table_name", T::Definition.table_name), 
    fmt::arg("where", where));

	Model::Log(query);
  sql << query, soci::use(&where_values), soci::into(r);

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
  soci::session sql = ModelFactory::getSession("default");
  soci::statement st(sql);
  soci::row rows;
  std::vector<T> ret;

	Model::Log(query);
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
  soci::session sql = ModelFactory::getSession("default");
  long count;

  soci::statement st(sql);

  Model::Log(query);
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
  soci::session sql = ModelFactory::getSession("default");

  std::string joined_columns;
  for (const auto &column : columns)
    joined_columns.append(", "+column.first+" "+column.second);

  std::string query;
  if (sql.get_backend_name() == "sqlite3")
    query = "create table if not exists {table_name} ("
      " {pkey_column} integer primary key {columns} )";
  else if (sql.get_backend_name() == "mysql")
    query = "create table if not exists {table_name} ("
      " {pkey_column} integer NOT NULL AUTO_INCREMENT {columns},"
      " PRIMARY KEY({pkey_column}) )";
  else 
    throw ModelException("Unrecognized backend. Unable to create table");

  query = fmt::format( query, 
    fmt::arg("table_name", T::Definition.table_name),
    fmt::arg("pkey_column", T::Definition.pkey_column),
    fmt::arg("columns", joined_columns));

	Model::Log(query);
	sql << query;
}

template <class T>
void Model::Instance<T>::DropTable() {
  soci::session sql = ModelFactory::getSession("default");

  // NOTE: I don't think there's any way to error test this, other than to expect
  // for an error to throw from soci...
	std::string query = fmt::format("drop table {table_name}",
    fmt::arg("table_name", T::Definition.table_name));
  sql << query;
}

