#include "prails_gtest.hpp"

#include <regex>

#include "tester_models.hpp"
#include "controller.hpp"

using namespace std;

PSYM_TEST_ENVIRONMENT()
PSYM_MODEL(TimeModel)
PSYM_MODEL(ValidationModel)
PSYM_MODEL(TesterModel)

class TesterModelTest : public PrailsControllerTest {
 public:
   static string driver;

 protected:
  Model::Record john_smith_record = {
    {"first_name", "John"},
    {"last_name", "Smith"},
    {"email", "jsmith@google.com"},
    {"password", nullopt},
    {"favorite_number", (long long int) 7},
    {"unlucky_number", (long long int) 3},
    {"is_enthusiastic",  (int) true},
    {"is_lazy", (int) false},
    {"untyped_string", "this wasnt in the constructor"}
  };

  // We use this to test out Validations that only reference the state of 'record'
  Model::Definition empty_definition {
    "id",
    "unavailable_table", 
    Model::ColumnTypes({{"id", COL_TYPE(long long int)}}),
    Model::Validations()
  };
  
  string DatabaseDriver() {
    // Set our local tests up for the right database driver:
    smatch matches;
    string dsn = PrailsControllerTest::config->dsn();
    if (regex_search(dsn, matches, regex("^([^\\:]+)")))
      return string(matches[1]);

    return string();
  }

  void create_one_hundred_hendersons() {
    for (unsigned int i = 0; i<100; i++) {
      TesterModel model_one(john_smith_record);
      model_one.first_name("John"+to_string(i));
      model_one.last_name("Henderson");
      model_one.email("jhenderson@yahoo.com");
      ASSERT_NO_THROW(model_one.save());
    }
  }

  void create_one_hundred_smiths() {
    for (unsigned int i = 0; i<100; i++) {
      TesterModel model_one(john_smith_record);
      model_one.first_name("John"+to_string(i));
      model_one.last_name("Smith");
      ASSERT_NO_THROW(model_one.save());
    }
  }
};

TEST_F(TesterModelTest, getters_and_setters) {
  TesterModel model(john_smith_record);

  EXPECT_EQ(model.isDirty(), true);
  EXPECT_EQ(model.id(), nullopt);
  EXPECT_EQ(model.first_name(), "John");
  EXPECT_EQ(model.last_name(), "Smith");
  EXPECT_EQ(model.email(), "jsmith@google.com");
  EXPECT_EQ(model.password(), nullopt);
  EXPECT_EQ(model.favorite_number(), 7);
  EXPECT_EQ(model.unlucky_number(), 3);
  EXPECT_EQ(model.is_enthusiastic(), true);
  EXPECT_EQ(model.is_lazy(), false);

  // We do want to support this, say, for the case of a calculated column in a 
  // custom query:
  EXPECT_EQ(get<string>(model.recordGet("untyped_string").value()), 
    string("this wasnt in the constructor"));

  model.first_name("Johnathan");
  EXPECT_EQ(model.first_name(), "Johnathan");

  model.last_name("Smitherson");
  EXPECT_EQ(model.last_name(), "Smitherson");

  model.email(nullopt);
  EXPECT_EQ(model.email(), nullopt);

  model.favorite_number(nullopt);
  EXPECT_EQ(model.favorite_number(), nullopt);

  EXPECT_TRUE(model.isDirty());
}

TEST_F(TesterModelTest, time_column) {
  auto tm_to_string = [] (tm from){ 
    char buffer [80];
    long int t = timegm(&from);
    strftime(buffer,80,"%c",gmtime(&t));
    return string(buffer);
  };

  string expected_time = "Tue Dec 10 14:04:27 2019";

  tm store_tm;
  store_tm.tm_sec = 27;
  store_tm.tm_min = 4;
  store_tm.tm_hour = 14;
  store_tm.tm_mday = 10;
  store_tm.tm_mon = 11;
  store_tm.tm_year = 119;
  store_tm.tm_zone = 0;
  store_tm.tm_gmtoff = 0;

  // NOTE: everything is stored in gmtime(), there is no zone info in the db

  // Test the above tm, before it enters the model:
  EXPECT_EQ(expected_time, tm_to_string(store_tm));

  TimeModel store_model({{"tested_at", store_tm}});

  // Test the recordGet on a ColumnTypes() declared column:
  EXPECT_EQ(expected_time, tm_to_string(*store_model.tested_at()));

  // Test the recordGet on a non-ColumnTypes() declared column:
  store_model.recordSet("new_field", store_tm);
  EXPECT_EQ(expected_time, 
    tm_to_string(get<tm>(*store_model.recordGet("new_field"))));

  EXPECT_NO_THROW(store_model.save());

  TimeModel retrieved_model = *TimeModel::Find(store_model.id().value());
 
  // Test the save() / Find():
  EXPECT_EQ(expected_time, tm_to_string(*retrieved_model.tested_at()));

  // Let's see what happens with a few other SQL query fields:
  // NOTE: For reasons unknown, it seems that soci has a bug here when we use
  //       soci to handle the id. So, I'll just use fmt here for now.
  auto tester_models = TimeModel::Select( fmt::format( 
    "select *, tested_at as other_at, {} as tested_at_string from time_models where id = :1",
    (DatabaseDriver() == "mysql") ? 
      "DATE_FORMAT(tested_at, '%Y-%m-%d %H:%i:%S')" :
      "strftime('%Y-%m-%d %H:%M:%S',tested_at)"), 
    (long) store_model.id().value() );

  EXPECT_EQ(tester_models.size(), 1);
  EXPECT_EQ(expected_time, tm_to_string(*tester_models[0].tested_at()));
  EXPECT_EQ(expected_time, 
    tm_to_string(get<tm>(*tester_models[0].recordGet("other_at"))));
  EXPECT_EQ("2019-12-10 14:04:27", 
    get<string>(*tester_models[0].recordGet("tested_at_string")));
  
  // Let's just test the accessor a bit, on a change
  // I'm not sure what this tests. I guess, the tested_at, and an update.
  store_tm.tm_mon = 2;
  retrieved_model.tested_at(store_tm);
  EXPECT_NO_THROW(retrieved_model.save());
  TimeModel retrieved_modelb = *TimeModel::Find(store_model.id().value());
  EXPECT_EQ("Sun Mar 10 14:04:27 2019", tm_to_string(*retrieved_modelb.tested_at()));
}

TEST_F(TesterModelTest, insert_and_load) {
  TesterModel model(john_smith_record);
  EXPECT_EQ(model.id(), nullopt);

  ASSERT_NO_THROW(model.save());

  EXPECT_TRUE(model.id() > 0);
  EXPECT_FALSE(model.isDirty());

  auto inserted_id = model.id().value();

  TesterModel retrieved = *TesterModel::Find(inserted_id);

  EXPECT_EQ(retrieved.isDirty(), false);
  EXPECT_EQ(retrieved.id(), inserted_id);
  EXPECT_EQ(retrieved.first_name(), "John");
  EXPECT_EQ(retrieved.last_name(), "Smith");
  EXPECT_EQ(retrieved.email(), "jsmith@google.com");
  EXPECT_EQ(retrieved.password(), nullopt);
  EXPECT_EQ(retrieved.favorite_number(), 7);
  EXPECT_EQ(retrieved.unlucky_number(), 3);
  EXPECT_EQ(retrieved.is_enthusiastic(), true);
  EXPECT_EQ(retrieved.is_lazy(), false);

  EXPECT_NO_THROW(retrieved.remove());
}

TEST_F(TesterModelTest, insert_and_update) {
  TesterModel model(john_smith_record);
  EXPECT_NO_THROW(model.save());
  EXPECT_TRUE(model.id() > 0);

  auto inserted_id = model.id().value();
  model.first_name("Johnathan");
  model.last_name("Smitherson");
  model.email(nullopt);
  model.favorite_number(nullopt);
  model.unlucky_number(13);
  model.is_lazy(true);
  EXPECT_EQ(model.isDirty(), true);
  EXPECT_EQ(model.isFromDatabase(), true);
  EXPECT_NO_THROW(model.save());

  TesterModel retrieved = *TesterModel::Find(inserted_id);
  EXPECT_EQ(retrieved.isDirty(), false);
  EXPECT_EQ(retrieved.id(), inserted_id);
  EXPECT_EQ(retrieved.first_name(), "Johnathan");
  EXPECT_EQ(retrieved.last_name(), "Smitherson");
  EXPECT_EQ(retrieved.email(), nullopt);
  EXPECT_EQ(retrieved.password(), nullopt);
  EXPECT_EQ(retrieved.favorite_number(), nullopt);
  EXPECT_EQ(retrieved.unlucky_number(), 13);
  EXPECT_EQ(retrieved.is_enthusiastic(), true);
  EXPECT_EQ(retrieved.is_lazy(), true);

  retrieved.last_name("Smitheroonie");
  EXPECT_EQ(retrieved.isDirty(), true);
  EXPECT_NO_THROW(retrieved.save());

  TesterModel retrieved_again = *TesterModel::Find(inserted_id);
  EXPECT_EQ(retrieved_again.isDirty(), false);
  EXPECT_EQ(retrieved_again.first_name(), "Johnathan");
  EXPECT_EQ(retrieved_again.last_name(), "Smitheroonie");
  EXPECT_EQ(retrieved_again.email(), nullopt);

  EXPECT_NO_THROW(retrieved_again.remove());
}

TEST_F(TesterModelTest, test_missing_record) {
  auto record = TesterModel::Find(2147483647);
  EXPECT_EQ(record, nullopt);
}

TEST_F(TesterModelTest, test_numeric_store_retrieve_limits) {
  // We're kind of also trying out the no-parameter constructor here
  TesterModel store_model;
  store_model.first_name("Testie");
  store_model.last_name("McNumber");
  store_model.ulong_test(numeric_limits<unsigned long>::max());
  store_model.unlucky_number(numeric_limits<unsigned long>::max());
  store_model.int_test(numeric_limits<int>::max());
  store_model.double_test(9.22337203685478e+18);

  EXPECT_NO_THROW(store_model.save());

  auto inserted_id = store_model.id().value();

  TesterModel retrieve_model = *TesterModel::Find(inserted_id);
  EXPECT_EQ(retrieve_model.first_name(), "Testie");
  EXPECT_EQ(retrieve_model.last_name(), "McNumber");

  EXPECT_EQ(*retrieve_model.unlucky_number(), numeric_limits<unsigned long>::max());
  EXPECT_EQ(*retrieve_model.ulong_test(), numeric_limits<unsigned long>::max());
  EXPECT_EQ(*retrieve_model.double_test(), 9.22337203685478e+18);
  EXPECT_EQ(*retrieve_model.int_test(), numeric_limits<int>::max());
  
  // Might as well test signed numbers out by multiplying by -1 and saving again
  store_model.int_test(*retrieve_model.int_test() * -1);
  store_model.double_test(*retrieve_model.double_test() * -1);
  EXPECT_NO_THROW(store_model.save());

  retrieve_model = *TesterModel::Find(inserted_id);
  EXPECT_EQ(*retrieve_model.double_test(), -1*9.22337203685478e+18);
  EXPECT_EQ(*retrieve_model.int_test(), -1*numeric_limits<int>::max());

  EXPECT_NO_THROW(retrieve_model.remove());
}

TEST(model_base_test, invalid_type_errors) {
  EXPECT_THROW(TesterModel({{"first_name", (long long int) 3}}), exception);

  // This should Also break
  TesterModel break_model_two;
  EXPECT_THROW(break_model_two.recordSet("first_name", (long long int) 3), exception);
}

TEST_F(TesterModelTest, test_delete) {
  // Member Function
  TesterModel model_one(john_smith_record);
  EXPECT_NO_THROW(model_one.save());

  auto model_one_id = model_one.id().value();
  EXPECT_NO_THROW(model_one.remove());
  EXPECT_EQ(TesterModel::Find(model_one_id), nullopt);

  // Static Class Function:
  TesterModel model_two(john_smith_record);
  EXPECT_NO_THROW(model_two.save());

  auto model_two_id = model_two.id().value();
  EXPECT_NO_THROW(TesterModel::Remove(model_two_id));
  EXPECT_EQ(TesterModel::Find(model_two_id), nullopt);

  // Test exception on delete of unpersisted record
  TesterModel model_three(john_smith_record);
  EXPECT_THROW(model_three.remove(), exception);
}

TEST_F(TesterModelTest, test_select_and_count) {
  create_one_hundred_hendersons();
  create_one_hundred_smiths();

  long henderson_count = TesterModel::Count(
    "select count(*) from tester_models where last_name = :1 and email = :2", 
      (string) "Henderson", (string) "jhenderson@yahoo.com");

  EXPECT_EQ(henderson_count, 100);

  long davidson_count = TesterModel::Count(
    "select count(*) from tester_models where last_name = :1", (string) "Davidson");

  EXPECT_EQ(davidson_count, 0);

  auto hendersons = TesterModel::Select(
    "select * from tester_models where last_name = :1 and email = :2", 
    (string) "Henderson", (string) "jhenderson@yahoo.com");

  ASSERT_EQ(hendersons.size(), 100);
  EXPECT_EQ(hendersons[0].first_name(), "John0");
  EXPECT_EQ(hendersons[99].first_name(), "John99");

  auto davidsons = TesterModel::Select(
    "select * from tester_models where last_name = :1", (string) "Davidson");

  EXPECT_EQ(davidsons.size(), 0);

  auto number_deleted = TesterModel::Execute("delete from tester_models");
  EXPECT_EQ(number_deleted, 200);
}

TEST_F(TesterModelTest, test_select_and_count_via_record_type) {
  create_one_hundred_hendersons();
  create_one_hundred_smiths();

  unsigned long henderson_count = TesterModel::Count(
    "select count(*) from tester_models where last_name = :last_name and email = :email", 
    Model::Record({ {"last_name", "Henderson"}, {"email", "jhenderson@yahoo.com"} }));

  EXPECT_EQ(henderson_count, 100);

  unsigned long davidson_count = TesterModel::Count(
    "select count(*) from tester_models where last_name = :lastname", 
    Model::Record({ {"last_name", "Henderson"} }));

  EXPECT_EQ(davidson_count, 0);

  auto hendersons = TesterModel::Select(
    "select * from tester_models where last_name = :last_name and email = :email", 
    Model::Record({ {"email", "jhenderson@yahoo.com"}, {"last_name", "Henderson"} }));

  ASSERT_EQ(hendersons.size(), 100);
  EXPECT_EQ(hendersons[0].first_name(), "John0");
  EXPECT_EQ(hendersons[99].first_name(), "John99");

  auto davidsons = TesterModel::Select(
    "select * from tester_models where last_name = :last_name", 
    Model::Record({ {"last_name", "Davidson"} }));

  EXPECT_EQ(davidsons.size(), 0);

  auto number_deleted = TesterModel::Execute("delete from tester_models");
  EXPECT_EQ(number_deleted, 200);
}

// This is mostly... bizarre. Since (sqlite, others?) doesnt support "= NULL" 
// we can't actually select 'where last_name = ?'. But, I wanted to test that
// the soci::to_base translates nullopt into NULL correctly, at least. So, I
// came up with this for now.... I guess it's kind of meaningless, but, it 
// at least documents the behavior...
TEST_F(TesterModelTest, test_null_in_record) {
  // Ensure that null values are properly translated...
  auto is_nulls = TesterModel::Select(
    "select 'is null works' where :test_value is null", 
    Model::Record({ {"test_value", nullopt} }));

  ASSERT_EQ(is_nulls.size(), 1);
  EXPECT_EQ(*is_nulls[0].recordGet("test_value"]), "is null works");
}

TEST_F(TesterModelTest, test_validator_not_null) {
  auto validator = Model::Validates::NotNull("field_name");
  auto error = Model::RecordError({"field_name", "is missing"});

  Model::Record r;
  r["field_name"] = "John";
  EXPECT_EQ(nullopt, validator.isValid(r,empty_definition));  

  r["field_name"] = string();
  EXPECT_EQ(nullopt, validator.isValid(r,empty_definition));  
  
  r["field_name"] = 12;
  EXPECT_EQ(nullopt, validator.isValid(r,empty_definition));  

  r["field_name"] = nullopt;
  EXPECT_EQ(error, *validator.isValid(r,empty_definition));

  Model::Record s = {{"irrelevent_field", "irrelevant value"}};
  EXPECT_EQ(error, *validator.isValid(s, empty_definition));
}

TEST_F(TesterModelTest, test_validator_not_empty) {
  auto validator = Model::Validates::NotEmpty("field_name");
  auto error = Model::RecordError({"field_name", "is empty"});

  Model::Record r;

  r["field_name"] = "John";
  EXPECT_EQ(nullopt, validator.isValid(r,empty_definition));  

  // No fail on null:
  r["field_name"] = nullopt;
  EXPECT_EQ(nullopt, validator.isValid(r,empty_definition));  

  Model::Record s = {{"irrelevent_field", "irrelevant value"}};
  EXPECT_EQ(nullopt, validator.isValid(s, empty_definition));

  r["field_name"] = string();
  EXPECT_EQ(error, *validator.isValid(r,empty_definition));

  // This is a type mismatch.
  // I suppose we could throw an exception- but, I think this should return "is empty":
  r["field_name"] = 12;
  EXPECT_EQ(error, *validator.isValid(r,empty_definition));
}

TEST_F(TesterModelTest, test_validator_is_bool) {
  auto validator = Model::Validates::IsBoolean("field_name");
  auto error = Model::RecordError({"field_name", "isnt a yes or no value"});

  Model::Record r;
  r["field_name"] = (int) 0;
  EXPECT_EQ(nullopt, validator.isValid(r,empty_definition));  

  r["field_name"] = (int) 1;
  EXPECT_EQ(nullopt, validator.isValid(r,empty_definition));  

  // No fail on null:
  r["field_name"] = nullopt;
  EXPECT_EQ(nullopt, validator.isValid(r,empty_definition));  

  Model::Record s = {{"irrelevent_field", "irrelevant value"}};
  EXPECT_EQ(nullopt, validator.isValid(s, empty_definition));

  r["field_name"] = (int) -1;
  EXPECT_EQ(error, *validator.isValid(r,empty_definition));

  r["field_name"] = (int) 2;
  EXPECT_EQ(error, *validator.isValid(r,empty_definition));

  r["field_name"] = (string) "Invalid Type";
  EXPECT_EQ(error, *validator.isValid(r,empty_definition));

}

TEST_F(TesterModelTest, test_validator_matches) {
  auto insensitive_john = Model::Validates::Matches("field_name", 
    regex_from_string("/john/i"));
  auto sensitive_john = Model::Validates::Matches("field_name",
    regex_from_string("/john/"));
  auto error = Model::RecordError({"field_name", "doesn't match the expected format"});

  Model::Record r;

  r["field_name"] = "John";
  EXPECT_EQ(nullopt, insensitive_john.isValid(r,empty_definition));  
  EXPECT_EQ(error, *sensitive_john.isValid(r,empty_definition));  

  // No fail on null:
  r["field_name"] = nullopt;
  EXPECT_EQ(nullopt, insensitive_john.isValid(r,empty_definition));  

  Model::Record s = {{"irrelevent_field", "irrelevant value"}};
  EXPECT_EQ(nullopt, sensitive_john.isValid(s, empty_definition));  

  r["field_name"] = 12;
  EXPECT_EQ(error, *sensitive_john.isValid(r,empty_definition));  
}

TEST_F(TesterModelTest, test_validator_string_column_unique) {
  // Let's create the first entry in the database:
  EXPECT_NO_THROW( ValidationModel(
    {{"email", "jsmith@google.com"}, {"is_lazy", (int) false}}
    ).save());
  
  ValidationModel model({{"email", "jsmith@google.com"}, {"is_lazy", (int) true}});

  EXPECT_FALSE(model.isValid());
  EXPECT_EQ( model.errors(), 
    Model::RecordErrors({ {"email", {"has already been registered"}}}));

  EXPECT_THROW(model.save(), exception);

  // Now fix the error, and see if we can save:
  model.email("jsmith2@google.com");
  EXPECT_TRUE(model.isValid());
  EXPECT_EQ( model.errors(), Model::RecordErrors({}) );
  EXPECT_NO_THROW(model.save());

  // Save a tangential field change just to make sure we don't trip the unique  
  model.is_lazy(false);
  EXPECT_TRUE(model.isValid());
  EXPECT_EQ( model.errors(), Model::RecordErrors({}) );
  EXPECT_NO_THROW(model.save());
}

TEST_F(TesterModelTest, test_validator_numeric_column_unique) {
  EXPECT_NO_THROW( ValidationModel({
    {"email", "charlie@google.com"},
    {"favorite_number", 23},
    {"is_lazy", (int) false}
    }).save());
  
  ValidationModel model({
    {"email", "juan@google.com"}, 
    {"favorite_number", 23}
  });

  EXPECT_FALSE(model.isValid());
  EXPECT_EQ( model.errors(), 
    Model::RecordErrors({ {"favorite_number", {"has already been registered"}}}));
  EXPECT_THROW(model.save(), exception);

  // Now fix the error, and see if we can save:
  model.favorite_number(38);
  EXPECT_TRUE(model.isValid());
  EXPECT_EQ( model.errors(), Model::RecordErrors({}) );
  EXPECT_NO_THROW(model.save());
}

TEST_F(TesterModelTest, test_validator_additional_where_unique) {
  EXPECT_NO_THROW( ValidationModel({
    {"email", "horton@google.com"},
    {"company_id", (int) 17},
    {"is_company_admin", (int) false}
    }).save());

  EXPECT_NO_THROW( ValidationModel({
    {"email", "louise@google.com"},
    {"company_id", (int) 17},
    {"is_company_admin", (int) false}
    }).save());

  EXPECT_NO_THROW( ValidationModel({
    {"email", "gina@google.com"},
    {"company_id", (int) 17},
    {"is_company_admin", (int) false}
    }).save());

  // Load louise, and make her a company admin.
  auto louise = *ValidationModel::Find("email = :email",{{"email", "louise@google.com"}});
  louise.is_company_admin(true);
  EXPECT_NO_THROW(louise.save());
  
  // Then load horton, and make sure we can't make horton an admin
  auto horton = *ValidationModel::Find("email = :email",{{"email", "horton@google.com"}});
  horton.is_company_admin(true);
  EXPECT_FALSE(horton.isValid());
  EXPECT_EQ( horton.errors(), 
    Model::RecordErrors({ {"is_company_admin", {"has already been registered"}}}));

  // Then, I guess, change the admin from one to the other
  louise.is_company_admin(false);
  EXPECT_NO_THROW(louise.save());

  horton.markDirty();

  EXPECT_TRUE(horton.isValid());
  EXPECT_NO_THROW(horton.save());
}

TEST_F(TesterModelTest, test_invalid_columns) {
  ValidationModel model({
    {"email", "ernie@google.com"},
    {"is_lazy", (int) false}
  });

  model.email("missing the at sign, thus invalid");

  // Fail on a non-matching email:
  EXPECT_FALSE(model.isValid());
  EXPECT_EQ( model.errors(), 
    Model::RecordErrors({ {"email", {"doesn't match the expected format"}}}));

  EXPECT_THROW(model.save(), exception);

  // Insert a now-valid model:
  model.email("ernie@google.com");
  EXPECT_TRUE(model.isValid());
  EXPECT_EQ( model.errors(), Model::RecordErrors({}) );

  EXPECT_NO_THROW(model.save());

  // Fail on a non-matching email:
  model.email(nullopt);

  EXPECT_FALSE(model.isValid());
  EXPECT_EQ( model.errors(), 
    Model::RecordErrors({ {"email", {"is missing"}}}));

  EXPECT_THROW(model.save(), exception);
  
  // Fail on a non-bool is_lazy:
  model.email("ernie@google.com");
  model.is_lazy(34);
  EXPECT_FALSE(model.isValid());
  
  EXPECT_EQ( model.errors(), 
    Model::RecordErrors({ {"is_lazy", {"isnt a yes or no value"}}}));

  // Update on a now-valid model
  model.is_lazy(0);

  EXPECT_TRUE(model.isValid());
  EXPECT_EQ( model.errors(), Model::RecordErrors({}) );
  EXPECT_NO_THROW(model.save());
}

TEST_F(TesterModelTest, to_json_test) {
  TesterModel store_model(john_smith_record);

  tm now = Model::NowUTC();
  store_model.updated_at(now);

  EXPECT_NO_THROW(store_model.save());

  TesterModel retrieved_model = *TesterModel::Find(store_model.id().value());

  auto json = Controller::ModelToJson(retrieved_model);

  EXPECT_TRUE(json["id"] > 0);
  EXPECT_EQ(json["first_name"], "John");
  EXPECT_EQ(json["last_name"], "Smith");
  EXPECT_EQ(json["email"], "jsmith@google.com");
  EXPECT_EQ(json["password"], nullptr); 
  EXPECT_EQ(json["favorite_number"], 7);
  EXPECT_EQ(json["unlucky_number"], 3);
  EXPECT_EQ(json["is_enthusiastic"], 1);
  EXPECT_EQ(json["is_lazy"], 0);
  EXPECT_EQ(json["updated_at"], tm_to_json(now));

  EXPECT_EQ(json["double_test"], nullptr); 
  EXPECT_EQ(json["int_test"], nullptr); 
  EXPECT_EQ(json["ulong_test"], nullptr); 

  EXPECT_EQ(json.size(), 13);
}

// In this test, we insert a record, with the id field set
TEST_F(TesterModelTest, test_insert_with_specified_id) {
  TesterModel model(john_smith_record);
  EXPECT_EQ(model.isDirty(), true);
  EXPECT_EQ(model.isFromDatabase(), false);
  model.id(673);
  EXPECT_EQ(model.isDirty(), true);
  EXPECT_EQ(model.isFromDatabase(), false);

  ASSERT_NO_THROW(model.save());
  EXPECT_EQ(model.isFromDatabase(), true);

  auto retrieved_model = TesterModel::Find(673);
  EXPECT_TRUE(retrieved_model.has_value());

  EXPECT_EQ((*retrieved_model).id(), 673);
}

// I'm not sure what the right thing to do is, here. But, this documents the
// current behavior.
TEST_F(TesterModelTest, test_id_change) {
  // Create a record:
  TesterModel model(john_smith_record);
  ASSERT_NO_THROW(model.save()); // this is an insert

  long first_id = *(model.id());

  // Now We change the id
  model.id(1234); 

  ASSERT_NO_THROW(model.save()); // This should also be an insert.

  auto retrieved_model = TesterModel::Find(1234);
  EXPECT_TRUE(retrieved_model.has_value());
  EXPECT_EQ((*retrieved_model).id(), 1234);

  auto retrieved_model2 = TesterModel::Find(first_id);
  EXPECT_TRUE(retrieved_model2.has_value());
  EXPECT_EQ((*retrieved_model2).id(), first_id);
}
