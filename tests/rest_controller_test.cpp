#include "prails_gtest.hpp"

// Seems like the GCC compiler doesn't like this in the rapidjson:
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#include "rapidjson/document.h"

#include "rest_controller_test.hpp"

using namespace std;
using namespace prails::utilities;

class TaskControllerFixture : public PrailsControllerTest {
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

const std::string TasksController::route_prefix = "/tasks";

INIT_MODEL_REGISTRY()
INIT_CONTROLLER_REGISTRY()

REGISTER_MODEL(Task)
REGISTER_CONTROLLER(TasksController)

INIT_PRAILS_TEST_ENVIRONMENT()

TEST_F(TaskControllerFixture, index) {

  for( unsigned int i = 0; i < 10; i++ ) {
    Task task(default_task);
    task.name("Task "+to_string(i));
    EXPECT_NO_THROW(task.save());
  }

  EXPECT_EQ(Task::Count("select count(*) from tasks"), 10);

  auto res = browser().Get("/tasks");

  EXPECT_EQ(res->status, 200);

  rapidjson::Document document;
  document.Parse(res->body.c_str());

  EXPECT_TRUE(document.IsArray());
  EXPECT_EQ(document.Size(), 10);

  for (rapidjson::SizeType i = 0; i < document.Size(); i++) {
    EXPECT_EQ(document[i]["active"].GetInt(), 1);
    EXPECT_EQ(document[i]["id"].GetInt(), i+1);
    EXPECT_EQ(string(document[i]["name"].GetString()), "Task "+to_string(i));
    EXPECT_EQ(string(document[i]["description"].GetString()), default_description);
    EXPECT_EQ(string(document[i]["created_at"].GetString()), epoch_as_jsontime);
    EXPECT_EQ(string(document[i]["updated_at"].GetString()), epoch_as_jsontime);
    EXPECT_EQ(document[i].MemberCount(), 6);
  }

  for (auto& t : Task::Select("select * from tasks")) t.remove();

  EXPECT_EQ(Task::Count("select count(*) from tasks"), 0);
}

TEST_F(TaskControllerFixture, read) {
  Task task(default_task);
  EXPECT_NO_THROW(task.save());

  auto res = browser().Get(fmt::format("/tasks/{}", *task.id()).c_str());

  EXPECT_EQ(res->status, 200);

  rapidjson::Document document;
  document.Parse(res->body.c_str());

  EXPECT_EQ(document["active"].GetInt(), 1);
  EXPECT_EQ(document["id"].GetInt(), *task.id());
  EXPECT_EQ(string(document["name"].GetString()), default_name);
  EXPECT_EQ(string(document["description"].GetString()), default_description);
  EXPECT_EQ(string(document["created_at"].GetString()), epoch_as_jsontime);
  EXPECT_EQ(string(document["updated_at"].GetString()), epoch_as_jsontime);

  task.remove();
}

TEST_F(TaskControllerFixture, create) {

  EXPECT_EQ(Task::Count("select count(*) from tasks"), 0);

  auto res = browser().Post("/tasks", 
    "name=Test+Task&description=lorem+ipsum+sit+dolor&active=1",
    "application/x-www-form-urlencoded");

  EXPECT_EQ(res->status, 200);

  rapidjson::Document document;
  document.Parse(res->body.c_str());

  unsigned int task_id = document["id"].GetInt();
  EXPECT_EQ(document["status"].GetInt(), 0);

  Task task = *Task::Find(task_id);

  EXPECT_EQ(*task.active(), 1);
  EXPECT_EQ(*task.id(), task_id);
  EXPECT_EQ(*task.name(), default_name);
  EXPECT_EQ(*task.description(), default_description);
  EXPECT_TRUE(tm_to_json(*task.created_at()).length() > 0);
  EXPECT_TRUE(tm_to_json(*task.updated_at()).length() > 0);

  task.remove();
}

TEST_F(TaskControllerFixture, update) {
  Task task(default_task);
  EXPECT_NO_THROW(task.save());

  auto res = browser().Put(
    fmt::format("/tasks/{}", *task.id()).c_str(), 
    "name=Updated+Task&description=updated+lorem+ipsum+sit+dolor&active=0",
    "application/x-www-form-urlencoded");

  EXPECT_EQ(res->status, 200);

  rapidjson::Document document;
  document.Parse(res->body.c_str());

  EXPECT_EQ(document["id"].GetInt(), *task.id());
  EXPECT_EQ(document["status"].GetInt(), 0);

  auto updated_task = *Task::Find(*task.id());

  EXPECT_EQ(*updated_task.active(), 0);
  EXPECT_EQ(*updated_task.id(), *task.id());
  EXPECT_EQ(*updated_task.name(), "Updated Task");
  EXPECT_EQ(*updated_task.description(), "updated lorem ipsum sit dolor");

  struct tm new_updated_at = *updated_task.updated_at();
  struct tm new_created_at = *updated_task.created_at();

  EXPECT_TRUE(difftime(mktime(&new_updated_at), mktime(&default_epoch)) > 0);
  EXPECT_TRUE(difftime(mktime(&new_created_at), mktime(&default_epoch)) == 0);

  updated_task.remove();
}

TEST_F(TaskControllerFixture, del) {
  Task task(default_task);
  EXPECT_NO_THROW(task.save());

  EXPECT_EQ(Task::Count("select count(*) from tasks"), 1);

  auto res = browser().Delete(fmt::format("/tasks/{}", *task.id()).c_str());

  EXPECT_EQ(res->status, 200);

  rapidjson::Document document;
  document.Parse(res->body.c_str());
  EXPECT_EQ(document["status"].GetInt(), 0);

  EXPECT_EQ(Task::Count("select count(*) from tasks"), 0);
}

TEST_F(TaskControllerFixture, multiple_update) {
  for( unsigned int i = 0; i < 4; i++ ) {
    Task task(default_task);
    task.name("Task "+to_string(i));
    EXPECT_NO_THROW(task.save());
  }

  auto tasks = Task::Select("select id from tasks");
  EXPECT_EQ(tasks.size(), 4);

  auto res = browser().Post("/tasks/multiple-update", fmt::format(
    "ids%5B%5D={}&ids%5B%5D={}&request%5Bdescription%5D=New+Description",
    *tasks[1].id(), *tasks[3].id() ), "application/x-www-form-urlencoded");

  EXPECT_EQ(res->status, 200);

  rapidjson::Document document;
  document.Parse(res->body.c_str());
  EXPECT_EQ(document["status"].GetInt(), 0);

  auto updated_tasks = Task::Select("select * from tasks");
  EXPECT_EQ(updated_tasks.size(), 4);

  for( unsigned int i = 0; i < updated_tasks.size(); i++ ) {
    auto task = updated_tasks[i];
    EXPECT_EQ(*task.active(), 1);
    EXPECT_EQ(*task.name(), "Task "+to_string(i));
    EXPECT_TRUE(*task.id() > 0);
    EXPECT_TRUE(tm_to_json(*task.created_at()).length() > 0);
    EXPECT_TRUE(tm_to_json(*task.updated_at()).length() > 0);

    if ( (*task.id() == *tasks[1].id()) || (*task.id() == *tasks[3].id()) )
      EXPECT_EQ(*task.description(), "New Description");
    else
      EXPECT_EQ(*task.description(), default_description);
  }

  for (auto& t : updated_tasks) t.remove();
}

TEST_F(TaskControllerFixture, multiple_delete) {
  for( unsigned int i = 0; i < 4; i++ ) {
    Task task(default_task);
    task.name("Task "+to_string(i));
    EXPECT_NO_THROW(task.save());
  }

  auto tasks = Task::Select("select id from tasks");
  EXPECT_EQ(tasks.size(), 4);

  auto res = browser().Post("/tasks/multiple-delete", fmt::format(
    "ids%5B%5D={}&ids%5B%5D={}&request%5Bdescription%5D=New+Description",
    *tasks[1].id(), *tasks[3].id() ), "application/x-www-form-urlencoded");

  EXPECT_EQ(res->status, 200);

  rapidjson::Document document;
  document.Parse(res->body.c_str());
  EXPECT_EQ(document["status"].GetInt(), 0);

  auto remaining_tasks = Task::Select("select * from tasks");
  EXPECT_EQ(remaining_tasks.size(), 2);

  for( auto &task: remaining_tasks )
    EXPECT_TRUE( (*task.id() == *tasks[0].id()) || (*task.id() == *tasks[2].id()) );

  for (auto& t : remaining_tasks) t.remove();
}
