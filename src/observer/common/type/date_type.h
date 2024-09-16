//
// Created by 闫润邦 on 2024/9/16.
//

#ifndef MINIDB_DATE_TYPE_H
#define MINIDB_DATE_TYPE_H

#include "common/rc.h"
#include "common/type/data_type.h"

struct Date_t {
  int year, month, day;
  static Date_t parse_date_from_string(const char *s);
  static bool is_valid_string_for_date(const char *s);
  bool operator < (const Date_t &rhs) const {
    if(year == rhs.year) {
      if(month == rhs.month) {
        return day < rhs.day;
      } else {
        return month < rhs.month;
      }
    } else {
      return year < rhs.year;
    }
  }
  bool operator == (const Date_t &rhs) const {
    return year == rhs.year && month == rhs.month && day == rhs.day;
  }
};

class DateType: public DataType {
public:
  DateType(): DataType(AttrType::DATES) {}
  virtual ~DateType() = default;

  int compare(const Value &left, const Value &right) const override;

  RC add(const Value &left, const Value &right, Value &result) const override;
  RC subtract(const Value &left, const Value &right, Value &result) const override;
  RC multiply(const Value &left, const Value &right, Value &result) const override;
  RC divide(const Value &left, const Value &right, Value &result) const override;
  RC negative(const Value &val, Value &result) const override;

  RC set_value_from_str(Value &val, const string &data) const override;

  RC to_string(const Value &val, string &result) const override;

  RC cast_to(const Value &val, AttrType type, Value &result) const override;

  int cast_cost(AttrType type) override;

};
#endif  // MINIDB_DATE_TYPE_H
