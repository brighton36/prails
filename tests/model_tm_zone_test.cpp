#include "prails_gtest.hpp"

#include "tester_models.hpp"
#include "controller.hpp"

using namespace std;

class TimeModelLocal : public Model::Instance<TimeModelLocal> { 
  public :
    using Model::Instance<TimeModelLocal>::Instance;

    MODEL_ACCESSOR(id, long long int)
    MODEL_ACCESSOR(tested_at, std::tm)

    inline static Model::Definition Definition {
      "id",
      "time_models_local",
       Model::ColumnTypes( {
         {"id", COL_TYPE(long long int)},
         {"tested_at", COL_TYPE(std::tm)}
      }),
      Model::Validations(),
      false // 'false' Persists time in local zone
    };

    static void Migrate(unsigned int version) { 
      if (version)
        CreateTable({ {"tested_at", "datetime"} }); 
      else
        DropTable();
    };

  private:
    static ModelRegister<TimeModelLocal> reg;
};

PSYM_TEST_ENVIRONMENT()
PSYM_MODEL(TimeModel)
PSYM_MODEL(TimeModelLocal)

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
      tm ret;
      memset(&ret, 0, sizeof(tm));

      // NOTE: strptime only sets the fields in the specifier string. That means no
      // isdst,gmtoff, or zone fields are set, and this tm will look like a UTC time.
      strptime(time_as_string.c_str(), "%Y-%m-%d %H:%M:%S", &ret);

      return ret;
    }

    tm local_tm(string time_as_string) {
      tm ret, zone_parts_tm;

      memset(&ret, 0, sizeof(tm));
      memset(&zone_parts_tm, 0, sizeof(tm));

      // NOTE: strptime only sets the fields in the specifier string. That means no
      // isdst,gmtoff, or zone fields are set, and this tm will look like a UTC time.
      strptime(time_as_string.c_str(), "%Y-%m-%d %H:%M:%S", &ret);

      memcpy(&zone_parts_tm, &ret, sizeof(tm));

      // NOTE: mktime seems to adjust the hour back one hour, on the provided
      // tm (zone_parts). But, gives us the local zone fields we need:
      mktime(&zone_parts_tm);

      ret.tm_gmtoff = zone_parts_tm.tm_gmtoff;
      ret.tm_isdst = zone_parts_tm.tm_isdst;
      ret.tm_zone = zone_parts_tm.tm_zone;

      return ret;
    }
};

string tm_to_string_with_zone(tm *time_as_tm) {
  char buffer[80];
  strftime(buffer,80,"%Y-%m-%d %H:%M:%S %z",time_as_tm);
  return string(buffer);
}

// This Sub-routine creates a T with the provided time, saves that model, then 
// loads it from the database. All transformations of the time are tested against
// expectations during that process. This is really the meat of this whole test
// suite.
template <class T>
void lifecycle_assertions(tm start_at, string end_at, string zone) {
  string end_at_with_zone = fmt::format("{} {}", end_at, zone);

  T create_model({{"tested_at", start_at}});
  tm created_epoch = create_model.tested_at().value();
  EXPECT_EQ(end_at_with_zone, tm_to_string_with_zone(&created_epoch));

  EXPECT_NO_THROW(create_model.save());

  vector<T> updated_models = T::Select(fmt::format(
      "select *, strftime('%Y-%m-%d %H:%M:%S',tested_at) as tested_at_string"
      " from {} where id = :id limit 1", T::Definition.table_name()),
      *create_model.id() );
  ASSERT_EQ(updated_models.size(), 1);

  ASSERT_TRUE(updated_models[0].tested_at().has_value());
  ASSERT_TRUE(updated_models[0].recordGet("tested_at_string").has_value());

  // This is so that we can ensure how the value is stored in the db itself:
  string tested_at_string = get<string>(updated_models[0].recordGet("tested_at_string").value());
  tm updated_epoch = updated_models[0].tested_at().value();

  EXPECT_EQ(end_at_with_zone, tm_to_string_with_zone(&updated_epoch));
  EXPECT_EQ(end_at, tested_at_string);
}

// Test that our fixture helpers actually work the way we expect
TEST_F(ModelTmZoneTest, fixture_test_features) { 
  setenv("TZ", "EST5EDT", 1);
  tzset();

  tm utc_time = utc_tm("2020-02-01 12:00:00");
  EXPECT_EQ(string("Sat Feb  1 12:00:00 2020\n"), string(asctime(&utc_time)));
  EXPECT_TRUE(utc_time.tm_zone == NULL);
  EXPECT_EQ(utc_time.tm_gmtoff, 0);
  EXPECT_EQ(utc_time.tm_isdst, 0);
  
  tm local_time = local_tm("2020-02-01 12:00:00");
  EXPECT_EQ(string("Sat Feb  1 12:00:00 2020\n"), string(asctime(&local_time)));
  EXPECT_EQ(string(local_time.tm_zone), string("EST"));
  EXPECT_EQ(local_time.tm_gmtoff, -18000);
  EXPECT_EQ(local_time.tm_isdst, 0);

  tm local_time_dst = local_tm("2020-05-01 12:00:00");
  EXPECT_EQ(string("Fri May  1 12:00:00 2020\n"), string(asctime(&local_time_dst)));
  EXPECT_EQ(string(local_time_dst.tm_zone), string("EDT"));
  EXPECT_EQ(local_time_dst.tm_gmtoff, -14400);
  EXPECT_EQ(local_time_dst.tm_isdst, 1);
}

// This is mostly just confirming my own knowledge of C++. But, 
// it should also help with portability.
TEST_F(ModelTmZoneTest, cpp_tm_features) { 
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

// This is mostly just confirming my own knowledge of C++. But, 
// it should also help with portability.
TEST_F(ModelTmZoneTest, cpp_tm_conversion) {
  setenv("TZ", "PST8PDT", 1);
  tzset();

  // UTC tm to time_t to UTC tm:
  tm epoch1_tm = utc_tm("2020-01-01 12:00:00");
  time_t epoch1_t = timegm(&epoch1_tm);
  tm epoch1_tm_adjusted;
  memcpy(&epoch1_tm_adjusted, gmtime(&epoch1_t), sizeof(tm));

  EXPECT_EQ("2020-01-01 12:00:00 +0000", tm_to_string_with_zone(&epoch1_tm_adjusted));
  EXPECT_EQ(epoch1_tm_adjusted.tm_zone, string("GMT"));
  EXPECT_EQ(epoch1_tm_adjusted.tm_gmtoff, 0);
  EXPECT_EQ(epoch1_tm_adjusted.tm_isdst, 0);

  // -800 tm to time_t to UTC tm:
  tm epoch2_tm = local_tm("2020-04-14 09:00:00");
  time_t epoch2_t = timelocal(&epoch2_tm);
  tm epoch2_tm_adjusted;

  // NOTE: I don't think gmtime adjusts the zone fields...
  memcpy(&epoch2_tm_adjusted, gmtime(&epoch2_t), sizeof(tm));

  EXPECT_EQ("2020-04-14 09:00:00 -0700", tm_to_string_with_zone(&epoch2_tm));
  EXPECT_EQ(string(epoch2_tm.tm_zone), string("PDT"));
  EXPECT_EQ(epoch2_tm.tm_gmtoff, -25200);
  EXPECT_EQ(epoch2_tm.tm_isdst, 1);

  EXPECT_EQ("2020-04-14 16:00:00 +0000", tm_to_string_with_zone(&epoch2_tm_adjusted));
  EXPECT_EQ(epoch2_tm_adjusted.tm_zone, string("GMT"));
  EXPECT_EQ(epoch2_tm_adjusted.tm_gmtoff, 0);
  EXPECT_EQ(epoch2_tm_adjusted.tm_isdst, 0);

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

  // local/est tm to time_t to utc tm:
  setenv("TZ", "EST5EDT", 1);
  tzset();

  tm epoch4_tm = local_tm("2020-02-14 09:00:00");
  EXPECT_EQ("2020-02-14 09:00:00 -0500", tm_to_string_with_zone(&epoch4_tm));
  time_t epoch4_t = timelocal(&epoch4_tm);
  tm epoch4_tm_adjusted;
  memcpy(&epoch4_tm_adjusted, gmtime(&epoch4_t), sizeof(tm));

  EXPECT_EQ("2020-02-14 14:00:00 +0000", tm_to_string_with_zone(&epoch4_tm_adjusted));
  EXPECT_EQ(epoch4_tm_adjusted.tm_zone, string("GMT"));
  EXPECT_EQ(epoch4_tm_adjusted.tm_gmtoff, 0);
  EXPECT_EQ(epoch4_tm_adjusted.tm_isdst, 0);
}

// This is mostly what our code concerns itself with. Basic persistance to the
// database, with outputs and state set to UTC.
TEST_F(ModelTmZoneTest, lifecycle_with_utc) {
  setenv("TZ", "PST8PDT", 1);
  tzset();

  { SCOPED_TRACE("TimeModel: UTC to UTC");
    lifecycle_assertions<TimeModel>( 
      utc_tm("2020-01-01 12:00:00"), "2020-01-01 12:00:00", "+0000"); }

  { SCOPED_TRACE("TimeModel: local/PST to UTC");
    lifecycle_assertions<TimeModel>(
      local_tm("2020-03-05 23:59:59"), "2020-03-06 07:59:59", "+0000"); }

  { SCOPED_TRACE("TimeModel: local/PDT to UTC");
    lifecycle_assertions<TimeModel>( 
      local_tm("2020-04-14 09:00:00"), "2020-04-14 16:00:00", "+0000"); }
}

// This is a near-repeat of the lifecycle_with_utc, albeit with 
// is_persisting_in_utc set to false in the TimeModelLocal
TEST_F(ModelTmZoneTest, lifecycle_with_local) {
  setenv("TZ", "PST8PDT", 1);
  tzset();

  { SCOPED_TRACE("TimeModelLocal: UTC to local/PST");
    lifecycle_assertions<TimeModelLocal>(
      utc_tm("2020-01-01 12:00:00"), "2020-01-01 04:00:00", "-0800"); }

  { SCOPED_TRACE("TimeModelLocal: UTC to local/PDT");
    lifecycle_assertions<TimeModelLocal>( 
      utc_tm("2020-06-30 00:00:00"), "2020-06-29 17:00:00", "-0700"); }

  { SCOPED_TRACE("TimeModelLocal: local/PST to local/PST");
    lifecycle_assertions<TimeModelLocal>(
      local_tm("2020-01-10 09:00:00"), "2020-01-10 09:00:00", "-0800"); }

  { SCOPED_TRACE("TimeModelLocal: local/PDT to local/PDT");
    lifecycle_assertions<TimeModelLocal>(
      local_tm("2020-07-01 12:00:00"), "2020-07-01 12:00:00", "-0700"); }
}
