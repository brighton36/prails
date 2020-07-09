#include <sstream>
#include "rest_controller.hpp"

class Task : public Model::Instance<Task> { 
  public:
    MODEL_CONSTRUCTOR(Task)

    MODEL_ACCESSOR(id, long)
    MODEL_ACCESSOR(name, std::string)
    MODEL_ACCESSOR(active, int)
    MODEL_ACCESSOR(description, std::string)
    MODEL_ACCESSOR(created_at, std::tm)
    MODEL_ACCESSOR(updated_at, std::tm)

    inline static const Model::Definition Definition {
      "id",
      "tasks", 
      Model::ColumnTypes({
        {"id",          COL_TYPE(long)},
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

    static void Migrate() {
      CreateTable({
        {"name", "varchar(100)"},
        {"active", "integer"},
        {"description", "varchar(300)"},
        {"created_at", "datetime"},
        {"updated_at", "datetime"},
      });
    }
};

class TasksController : public Controller::RestInstance<TasksController, Task> { 
  public:
    static const std::string route_prefix;

    using Controller::RestInstance<TasksController, Task>::RestInstance;

  private:
    Task modelDefault(std::tm tm_time) {
      return Task({ {"created_at", tm_time}, {"active", (int) 1} });
    }

    void modelUpdate(Task &task, Controller::PostBody &post, std::tm tm_time) {
      task.updated_at(tm_time);

      if (post["name"]) task.name(*post["name"]);
      if (post["description"]) task.description(*post["description"]);
      if (post["active"]) task.active(stoi(*post["active"]));
    }

    static ControllerRegister<TasksController> reg;
};

class TaskControllerFixture : public ::testing::Test {
 public:

 protected:
  std::string default_name = "Test Task";
  std::string default_description = "lorem ipsum sit dolor";
  std::string epoch_as_jsontime = "2020-04-14T16:35:12.0+0000";
  struct tm default_epoch = DefaultEpoch();

  Model::Record default_task = {
		{"name",        "Test Task"},
		{"active",      (int) true},
		{"description", default_description},
		{"updated_at",  default_epoch},
		{"created_at",  default_epoch}
	};

  static tm DefaultEpoch() {
    struct tm ret;
    std::istringstream ss("2020-04-14 16:35:12");
    ss >> std::get_time(&ret, "%Y-%m-%d %H:%M:%S");
    return ret;
  }
};
