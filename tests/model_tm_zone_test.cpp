#include "prails_gtest.hpp"

#include "tester_models.hpp"
#include "controller.hpp"
#include <iostream> // TODO

using namespace std;

class TimeModelEDT : public Model::Instance<TimeModelEDT> { 
  public :
    MODEL_CONSTRUCTOR(TimeModelEDT)
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
      -5*3600 // Eastern non-dst zone
    };

    static void Migrate() {
      CreateTable({ {"tested_at", "datetime"} });
    };

  private:
    static ModelRegister<TimeModelEDT> reg;
};

INIT_MODEL_REGISTRY()
INIT_CONTROLLER_REGISTRY()

REGISTER_MODEL(TimeModel)
REGISTER_MODEL(TimeModelEDT)

INIT_PRAILS_TEST_ENVIRONMENT()

class ModelTmZoneTest : public PrailsControllerTest {
  protected:
    tm string_with_zone_to_tm(string time_as_string, long int gmtoff = 0) {
      struct tm ret;
      memset(&ret, 0, sizeof(tm));
      istringstream(time_as_string) >> get_time(&ret, "%Y-%m-%d %H:%M:%S");
      ret.tm_gmtoff = gmtoff;
      return ret;
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

TEST_F(ModelTmZoneTest, lifecycle_with_utc) {
  // Time provided in UTC, with UTC persist_at_gmt_offset
  struct tm create_epoch = string_with_zone_to_tm("2020-04-14 16:35:12", 0);

  TimeModel create_model({{"tested_at", create_epoch}});
  EXPECT_NO_THROW(create_model.save());

  auto updated_model = *TimeModel::Find(*create_model.id());
  auto updated_epoch = *updated_model.tested_at();

  EXPECT_EQ(tm_to_string_with_zone(&updated_epoch), tm_to_string_with_zone(&create_epoch));
  EXPECT_EQ(updated_epoch.tm_gmtoff, create_epoch.tm_gmtoff);
  EXPECT_EQ(updated_epoch.tm_isdst, 0);
  EXPECT_EQ(create_epoch.tm_isdst, 0);
  
  // TODO: Time provided in EDT, with UTC persist_at_gmt_offset

  // TODO: Time provided in PDT, with UTC persist_at_gmt_offset

  // TODO: Test the case of a tm queried from the database, when there's no column
  // so, I guess, an (tested_at+6 hours) as undefined_time_column_at
}

TEST_F(ModelTmZoneTest, lifecycle_with_est) {
  // Time provided in EDT, with EDT persist_at_gmt_offset
  struct tm create_epoch = string_with_zone_to_tm("2020-04-14 16:35:12", -5 * 3600);

  TimeModelEDT create_model({{"tested_at", create_epoch}});
  EXPECT_NO_THROW(create_model.save());

  auto updated_model = *TimeModelEDT::Find(*create_model.id());
  auto updated_epoch = *updated_model.tested_at();

  EXPECT_EQ(tm_to_string_with_zone(&updated_epoch), tm_to_string_with_zone(&create_epoch));
  EXPECT_EQ(updated_epoch.tm_gmtoff, create_epoch.tm_gmtoff);
  EXPECT_EQ(updated_epoch.tm_isdst, 0);
  EXPECT_EQ(create_epoch.tm_isdst, 0);

  // Eastern Zone, with DST in effect

  // TODO: Time provided in UTC, with EDT persist_at_gmt_offset

  // TODO: Time provided in PDT, with EDT persist_at_gmt_offset

  // TODO: Test the case of a tm queried from the database, when there's no column
  // so, I guess, a (tested_at+6 hours) as undefined_time_column_at
}

