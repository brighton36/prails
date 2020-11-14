#include "prails_gtest.hpp"

#include "tester_models.hpp"
#include "controller.hpp"
#include <iostream> // TODO

using namespace std;

class TimeModelLocal : public Model::Instance<TimeModelLocal> { 
  public :
    MODEL_CONSTRUCTOR(TimeModelLocal)
    MODEL_ACCESSOR(id, long)
    MODEL_ACCESSOR(tested_at, std::tm)

    inline static const Model::Definition Definition {
      "id",
      "time_models_edt", 
      Model::ColumnTypes( { 
        {"id",        COL_TYPE(long)},
        {"tested_at", COL_TYPE(std::tm)}
      }),
      Model::Validations(),
      false // Persists time in local zone
    };

    static void Migrate() {
      CreateTable({ {"tested_at", "datetime"} });
    };

  private:
    static ModelRegister<TimeModelLocal> reg;
};

INIT_MODEL_REGISTRY()
INIT_CONTROLLER_REGISTRY()

REGISTER_MODEL(TimeModel)
REGISTER_MODEL(TimeModelLocal)

INIT_PRAILS_TEST_ENVIRONMENT()

class ModelTmZoneTest : public PrailsControllerTest {
  public:
    void SetUp() override {
      // I think this is just overkill. But, we put things back the way we
      // found them:
      if (char* tz_sz = getenv("TZ"); tz_sz != NULL) 
        starting_tz = string(tz_sz);

      PrailsControllerTest::SetUp();
    }

    void TearDown() override {
      PrailsControllerTest::TearDown();

      // Reset the TZ environment:
      if (starting_tz.empty())
        unsetenv("TZ");
      else
        setenv("TZ", starting_tz.c_str(), 1);

      tzset();
    }

  protected:
    string starting_tz;

    tm utc_tm(string time_as_string) {
      // TODO: Clean this up...
      struct tm ret;
      memset(&ret, 0, sizeof(tm));
      istringstream(time_as_string) >> get_time(&ret, "%Y-%m-%d %H:%M:%S");
      return ret;
    }

    tm local_tm(string time_as_string) {
      // TODO: Clean this up...
      struct tm zoneless_tm;
      struct tm local_tm;
      memset(&zoneless_tm, 0, sizeof(tm));
      memset(&local_tm, 0, sizeof(tm));

      strptime(time_as_string.c_str(), "%Y-%m-%d %H:%M:%S", &zoneless_tm);
      time_t zoneless_t = mktime(&zoneless_tm);

      memcpy(&local_tm, localtime(&zoneless_t), sizeof(tm));

      return local_tm;
    }

    string tm_to_string_with_zone(tm *time_as_tm) {
      char buffer[80];
      strftime(buffer,80,"%Y-%m-%d %H:%M:%S %z",time_as_tm);
      return string(buffer);
    }
};

TEST_F(ModelTmZoneTest, cpp_tm_features) { 
  // This is mostly just confirming my own knowledge of C++. But, 
  // it should also help with portability.
  setenv("TZ", "EST5EDT", 1);
  tzset();

  // November 5th, 2020. Not in dst.
  time_t epoch1 = 1604609654; 

  // September 6th, 2020. During dst.
  time_t epoch2 = epoch1 - 3600*24*60; 

  tm utc_tm1, edt_tm1, utc_tm2, edt_tm2;
  memcpy(&utc_tm1, gmtime(&epoch1), sizeof(tm));
  memcpy(&edt_tm1, localtime(&epoch1), sizeof(tm));
  memcpy(&utc_tm2, gmtime(&epoch2), sizeof(tm));
  memcpy(&edt_tm2, localtime(&epoch2), sizeof(tm));

  EXPECT_EQ("2020-11-05 20:54:14 +0000", tm_to_string_with_zone(&utc_tm1));
  EXPECT_EQ("2020-11-05 15:54:14 -0500", tm_to_string_with_zone(&edt_tm1));
  EXPECT_EQ("2020-09-06 20:54:14 +0000", tm_to_string_with_zone(&utc_tm2));
  EXPECT_EQ("2020-09-06 16:54:14 -0400", tm_to_string_with_zone(&edt_tm2));

  EXPECT_EQ(utc_tm1.tm_isdst, 0);
  EXPECT_EQ(edt_tm1.tm_isdst, 0);
  EXPECT_EQ(utc_tm2.tm_isdst, 0);
  EXPECT_EQ(edt_tm2.tm_isdst, 1); 

  EXPECT_EQ(utc_tm1.tm_gmtoff, 0);
  EXPECT_EQ(edt_tm1.tm_gmtoff, -5 * 3600);
  EXPECT_EQ(utc_tm2.tm_gmtoff, 0);
  // NOTE : The offset here is a DST offset. -4, instead of -5
  EXPECT_EQ(edt_tm2.tm_gmtoff, -4 * 3600); 

  // NOTE: mktime ignores the tm_gmtoff, and assumes the system time zone
  // timegm assumes utc
  EXPECT_EQ(timegm(&utc_tm1), epoch1);
  EXPECT_EQ(timegm(&utc_tm2), epoch2);
  EXPECT_EQ(mktime(&edt_tm1), epoch1);
  EXPECT_EQ(mktime(&edt_tm2), epoch2);

  // This is just extra credit. I'm trying to break the libraries here...
  
  // timegm: Seems to ignore the tm_isdst and tm_gmtoff field:
  utc_tm1.tm_isdst = 1;
  utc_tm1.tm_gmtoff = 12*3600;
  EXPECT_EQ(timegm(&utc_tm1), epoch1);

  utc_tm2.tm_isdst = 1;
  utc_tm2.tm_gmtoff = 12*3600;
  EXPECT_EQ(timegm(&utc_tm2), epoch2);

  // mktime: it seems that tm_isdst does affect the conversion. 
  // I guess it adds/subtracts an hour in (my) zone:
  edt_tm2.tm_isdst = 0;
  EXPECT_EQ(mktime(&edt_tm2), epoch2+3600);

  edt_tm1.tm_isdst = 1;
  EXPECT_EQ(mktime(&edt_tm1), epoch1-3600);
}

TEST_F(ModelTmZoneTest, cpp_tm_conversion) {
  // This is mostly just confirming my own knowledge of C++. But, 
  // it should also help with portability.
  setenv("TZ", "PST8PDT", 1);
  tzset();

  // UTC tm to time_t to UTC tm:
  tm epoch1_tm = utc_tm("2020-01-01 12:00:00");
  time_t epoch1_t = timegm(&epoch1_tm);
  tm epoch1_tm_adjusted;
  memcpy(&epoch1_tm_adjusted, gmtime(&epoch1_t), sizeof(tm));

  // -800 tm to time_t to UTC tm:
  tm epoch2_tm = local_tm("2020-04-14 09:00:00");
  time_t epoch2_t = timelocal(&epoch2_tm);
  tm epoch2_tm_adjusted;
  memcpy(&epoch2_tm_adjusted, gmtime(&epoch2_t), sizeof(tm));

  EXPECT_EQ("2020-04-14 17:00:00 +0000", tm_to_string_with_zone(&epoch2_tm_adjusted));
  
  // local/pdt tm to time_t to local/pdt tm:
  tm epoch3_tm = local_tm("2020-11-12 01:00:00");
  time_t epoch3_t = mktime(&epoch3_tm);
  tm epoch3_tm_adjusted;
  memcpy(&epoch3_tm_adjusted, localtime(&epoch3_t), sizeof(tm));
  EXPECT_EQ("2020-11-12 01:00:00 -0800", tm_to_string_with_zone(&epoch3_tm_adjusted));

  // Arbitrary now_t, to pdt and utc:
  time_t now_t = 1605306270; // time(&now_t) == 2020-11-13 22:24:30 +0000
  tm now_tm, now_tm_utc;

  memcpy(&now_tm, localtime(&now_t), sizeof(tm));
  memcpy(&now_tm_utc, gmtime(&now_t), sizeof(tm));

  EXPECT_EQ("2020-11-13 14:24:30 -0800", tm_to_string_with_zone(&now_tm));
  EXPECT_EQ("2020-11-13 22:24:30 +0000", tm_to_string_with_zone(&now_tm_utc));
}

TEST_F(ModelTmZoneTest, lifecycle_with_utc) {
  setenv("TZ", "PST8PDT", 1);
  tzset();
  
  // Time provided in UTC, with UTC gmt_offset
  TimeModel create_model({{"tested_at", utc_tm("2020-01-01 12:00:00 +0000")}});
  tm created_epoch = *create_model.tested_at();
  EXPECT_NO_THROW(create_model.save());

  TimeModel updated_model = *TimeModel::Find(*create_model.id());

  tm updated_epoch = *updated_model.tested_at();

  EXPECT_EQ("2020-01-01 12:00:00 +0000", tm_to_string_with_zone(&created_epoch));
  EXPECT_EQ("2020-01-01 12:00:00 +0000", tm_to_string_with_zone(&updated_epoch));

  // -800 tm to time_t to UTC tm via is_persisting_in_utc:
  std::tm test2_tm = local_tm("2020-04-14 09:00:00");

  TimeModel create_model2({{"tested_at", test2_tm}});
  tm created_epoch2 = *create_model2.tested_at();
  EXPECT_NO_THROW(create_model2.save());

  TimeModel updated_model2 = *TimeModel::Find(*create_model2.id());
  tm updated_epoch2 = *updated_model2.tested_at();

  EXPECT_EQ("2020-04-14 17:00:00 +0000", tm_to_string_with_zone(&created_epoch2));
  EXPECT_EQ("2020-04-14 17:00:00 +0000", tm_to_string_with_zone(&updated_epoch2));

  // TODO: Test the case of a tm queried from the database, when there's no column
  // so, I guess, an (tested_at+6 hours) as undefined_time_column_at
  
  // TODO: Test a select where we convert the date to string, and ensure its stored in teh correct zone

  // TODO: store a EST , retrieve in pST
  //setenv("TZ", "PST8PDT", 1);
  //tzset();
}

/*
TEST_F(ModelTmZoneTest, lifecycle_with_est) {
  // Time provided in EDT, with EDT gmt_offset
  struct tm create_epoch = string_with_zone_to_tm("2020-04-14 16:35:12", -5 * 3600);

  //printf("create_epoch: %s %ld %d\n", create_epoch.tm_zone, create_epoch.tm_gmtoff, create_epoch.tm_isdst);
  TimeModelLocal create_model({{"tested_at", create_epoch}});
  EXPECT_NO_THROW(create_model.save());

  auto updated_model = *TimeModelLocal::Find(*create_model.id());
  auto updated_epoch = *updated_model.tested_at();

  EXPECT_EQ(tm_to_string_with_zone(&updated_epoch), tm_to_string_with_zone(&create_epoch));
  EXPECT_EQ(updated_epoch.tm_gmtoff, create_epoch.tm_gmtoff);
  EXPECT_EQ(updated_epoch.tm_isdst, 0);
  EXPECT_EQ(create_epoch.tm_isdst, 0);

  // Eastern Zone, with DST in effect

  // TODO: Time provided in UTC, with EDT gmt_offset

  // TODO: Time provided in PDT, with EDT persist_at_gmt_offset

  // TODO: Test the case of a tm queried from the database, when there's no column
  // so, I guess, a (tested_at+6 hours) as undefined_time_column_at
}
*/
