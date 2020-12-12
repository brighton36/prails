#include "model.hpp"

using namespace prails::utilities;

class TimeModel : public Model::Instance<TimeModel> { 
  public :
    using Model::Instance<TimeModel>::Instance;

    MODEL_ACCESSOR(id, long)
    MODEL_ACCESSOR(tested_at, std::tm)

    inline static const Model::Definition Definition {
      "id",
      "time_models", 
      Model::ColumnTypes( { 
        {"id",        COL_TYPE(long)},
        {"tested_at", COL_TYPE(std::tm)}
      }),
      Model::Validations()
    };

    static void Migrate() {
      CreateTable({ {"tested_at", "datetime"} });
    };

  private:
    static ModelRegister<TimeModel> reg;
};

class ValidationModel : public Model::Instance<ValidationModel> { 
  public :
    using Model::Instance<ValidationModel>::Instance;

    MODEL_ACCESSOR(id, long)
    MODEL_ACCESSOR(email, std::string)
    MODEL_ACCESSOR(is_lazy, int)
    MODEL_ACCESSOR(is_company_admin, int)
    MODEL_ACCESSOR(favorite_number, long)
    MODEL_ACCESSOR(company_id, long)

    inline static const Model::Definition Definition {
      "id",
      "validation_models", 
      Model::ColumnTypes( { 
        {"id",      COL_TYPE(long)},
        {"email",   COL_TYPE(std::string)},
        {"favorite_number", COL_TYPE(long)},
        {"company_id", COL_TYPE(long)},
        {"is_company_admin", COL_TYPE(int)},
        {"is_lazy", COL_TYPE(int)}
      }),
      Model::Validations( {
        Model::Validates::NotNull("email"),
        Model::Validates::Matches("email", regex_from_string("/.+@.+/")),
        Model::Validates::IsUnique("email"),
        Model::Validates::IsUnique("favorite_number"),
        Model::Validates::IsBoolean("is_company_admin"),
        Model::Validates::IsUnique("is_company_admin", 
          [](Model::Record record){ 
            return (record["company_id"] && record["company_id"].has_value()) ?
              std::make_optional(Model::Conditional( 
                "company_id = :company_id and is_company_admin = :is_company_admin", {
                  {"company_id", record["company_id"]},
                  {"is_company_admin", (int) true}
                })
            ) : std::optional<Model::Conditional>(std::nullopt);
          }),
        Model::Validates::IsBoolean("is_lazy")
      })
    };

    static void Migrate() {
      CreateTable({
        {"email", "varchar(100)"},
        {"favorite_number", "integer"},
        {"company_id", "integer"},
        {"is_company_admin", "integer"},
        {"is_lazy", "integer"},
      });
    };

  private:
    static ModelRegister<ValidationModel> reg;
};

class TesterModel : public Model::Instance<TesterModel> { 
  public:
    using Model::Instance<TesterModel>::Instance;

    MODEL_ACCESSOR(id, long)
    MODEL_ACCESSOR(first_name, std::string)
    MODEL_ACCESSOR(last_name, std::string)
    MODEL_ACCESSOR(email, std::string)
    MODEL_ACCESSOR(password, std::string)
    MODEL_ACCESSOR(favorite_number, long)
    MODEL_ACCESSOR(unlucky_number, long)
    MODEL_ACCESSOR(double_test, double)
    MODEL_ACCESSOR(ulong_test, unsigned long)
    MODEL_ACCESSOR(int_test, int)
    MODEL_ACCESSOR(is_enthusiastic, int)
    MODEL_ACCESSOR(is_lazy, int)
    MODEL_ACCESSOR(updated_at, std::tm)

    inline static const Model::Definition Definition {
      "id",
      "tester_models", 
      Model::ColumnTypes({
        {"id",              COL_TYPE(long)},
        {"first_name",      COL_TYPE(std::string)},
        {"last_name",       COL_TYPE(std::string)},
        {"email",           COL_TYPE(std::string)},
        {"password",        COL_TYPE(std::string)},
        {"favorite_number", COL_TYPE(long)},
        {"unlucky_number",  COL_TYPE(long)},
        {"double_test",     COL_TYPE(double)},
        {"ulong_test",      COL_TYPE(unsigned long)},
        {"int_test",        COL_TYPE(int)},
        {"is_enthusiastic", COL_TYPE(int)},
        {"is_lazy",         COL_TYPE(int)},
        {"updated_at",      COL_TYPE(std::tm)}
      }),
      Model::Validations({})
    }; 

    // Database Driver Concerns:
    static void Migrate() {
      CreateTable({
        {"first_name", "varchar(100)"},
        {"last_name", "varchar(100)"},
        {"email", "varchar(100)"},
        {"password", "varchar(100)"},
        {"favorite_number", "integer"},
        {"unlucky_number", "integer"},
        {"double_test", "real"},
        {"ulong_test", "integer"},
        {"int_test", "integer"},
        {"is_enthusiastic", "integer"},
        {"is_lazy", "integer"},
        {"updated_at", "datetime"}
      });
    };

  private:
    static ModelRegister<TesterModel> reg;
};
