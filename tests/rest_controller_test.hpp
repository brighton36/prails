#include <sstream>
#include "rest_controller.hpp"
#include "model_factory.hpp"

class Task : public Model::Instance<Task> { 
  public:
    using Model::Instance<Task>::Instance;

    MODEL_ACCESSOR(id, long long int)
    MODEL_ACCESSOR(name, std::string)
    MODEL_ACCESSOR(active, int)
    MODEL_ACCESSOR(description, std::string)
    MODEL_ACCESSOR(created_at, std::tm)
    MODEL_ACCESSOR(updated_at, std::tm)

    inline static Model::Definition Definition {
      "id",
      "tasks", 
      Model::ColumnTypes({
        {"id",          COL_TYPE(long long int)},
        {"name",        COL_TYPE(std::string)},
        {"active",      COL_TYPE(int)},
        {"description", COL_TYPE(std::string)},
        {"created_at",  COL_TYPE(std::tm)},
        {"updated_at",  COL_TYPE(std::tm)}
      }),
      Model::Validations( {
        Model::Validates::NotNull("name"),
        Model::Validates::NotEmpty("name"),
        Model::Validates::NotNull("active"),
        Model::Validates::IsBoolean("active"),
        Model::Validates::NotNull("created_at"),
        Model::Validates::NotNull("updated_at")
      })
    };

    static void Migrate(unsigned int version) {
      if (version)
        CreateTable({
          {"name", "varchar(100)"},
          {"active", "integer"},
          {"description", "varchar(300)"},
          {"created_at", "datetime"},
          {"updated_at", "datetime"},
        });
      else
        DropTable();
    }

  private:
    // NOTE: This isn't used in our rest_controller_test, but I included it here
    // so that we can include this in the vuecrudd:
    static ModelRegister<Task> reg;
};

// This is a courtesy to the vuecrud demo. Keeps this DRY.
#ifndef TASKS_REST_PREFIX
#define TASKS_REST_PREFIX "/tasks"
#endif

#ifndef TASKS_CLASS_NAME
#define TASKS_CLASS_NAME TasksController
#endif

#ifndef AUTHORIZER_CLASS_NAME
#define AUTHORIZER_CLASS_NAME Controller::AuthorizeAll
#endif

class TASKS_CLASS_NAME : 
public Controller::RestInstance<TASKS_CLASS_NAME, Task, AUTHORIZER_CLASS_NAME> { 
  public:
    static constexpr std::string_view rest_prefix = { TASKS_REST_PREFIX };

    using Controller::RestInstance<TASKS_CLASS_NAME, Task, AUTHORIZER_CLASS_NAME>::RestInstance;

  private:
    Task model_default(std::tm tm_time, AUTHORIZER_CLASS_NAME &) {
      return Task({ {"created_at", tm_time}, {"active", (int) 1} });
    }

    void model_update(Task &task, Controller::PostBody &post, std::tm tm_time, AUTHORIZER_CLASS_NAME &) {
      task.updated_at(tm_time);

      if (post["name"]) task.name(*post["name"]);
      if (post["description"]) task.description(*post["description"]);
      if (post["active"]) task.active(stoi(*post["active"]));
    }

    static ControllerRegister<TASKS_CLASS_NAME> reg;
};

