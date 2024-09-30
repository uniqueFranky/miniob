//
// Created by 闫润邦 on 2024/9/16.
//

#include "date_type.h"
#include "common/log/log.h"
#include "common/value.h"
#include <iomanip>

int DateType::compare(const Value &left, const Value &right) const {
  ASSERT(left.attr_type_ == AttrType::DATES && right.attr_type_ == AttrType::DATES, "invalid type");
  Date_t lval = left.get_date();
  Date_t rval = right.get_date();
  Value v1(lval), v2(rval);
  if(lval == rval) {
    return 0;
  } else if(lval < rval) {
    return -1;
  } else {
    return 1;
  }
}

RC DateType::add(const Value &left, const Value &right, Value &result) const
{
  return RC::UNSUPPORTED;
}

RC DateType::subtract(const Value &left, const Value &right, Value &result) const
{
  return RC::UNSUPPORTED;
}

RC DateType::multiply(const Value &left, const Value &right, Value &result) const
{
  return RC::UNSUPPORTED;
}

RC DateType::divide(const Value &left, const Value &right, Value &result) const
{
  return RC::UNSUPPORTED;
}

RC DateType::negative(const Value &val, Value &result) const
{
  return RC::UNSUPPORTED;
}

RC DateType::set_value_from_str(Value &val, const string &data) const
{
  if(!Date_t::is_valid_string_for_date(data.c_str())) {
    return RC::INVALID_ARGUMENT;
  }
  val.set_date(Date_t::parse_date_from_string(data.c_str()));
  return RC::SUCCESS;
}

RC DateType::to_string(const Value &val, string &result) const
{
  std::stringstream ss;
  Date_t date = val.get_date();
  ss << std::setw(4) << std::setfill('0') << date.year << "-" << std::setw(2) << std::setfill('0') << date.month << "-" << std::setw(2) << std::setfill('0') << date.day;
  ss >> result;
  return RC::SUCCESS;
}

RC DateType::cast_to(const Value &val, AttrType type, Value &result) const
{
  switch(type) {
    case AttrType::CHARS: {
      std::string s;
      RC rc = DataType::type_instance(AttrType::DATES)->to_string(val, s);
      if(OB_FAIL(rc)) {
        LOG_WARN("Failed to convert date to string.");
        return rc;
      }
      result.set_string(s.c_str(), s.length());
      return RC::SUCCESS;
    } break;
    default: {
      return RC::UNSUPPORTED;
    }
  }
}

int DateType::cast_cost(AttrType type)
{
  switch(type) {
    case AttrType::DATES: {
      return 0;
    } break;
    case AttrType::CHARS: {
      // LogicalPlanGenerator::create_plan 中会选择花费较小的转换。
      // 若日期和字符串进行比较，我们期望将字符串转成日期
      return 10000;
    } break;
    default: {
      return INT32_MAX;
    }
  }
}

Date_t Date_t::parse_date_from_string(const char *s)
{
  Date_t date;
  int i = 0;
  // parse year
  int num = 0;
  while(i < strlen(s) && s[i] != '-') {
    num = num * 10 + s[i] - '0';
    i++;
  }
  date.year = num;
  // parse month
  i++;
  num = 0;
  while(i < strlen(s) && s[i] != '-') {
    num = num * 10 + s[i] - '0';
    i++;
  }
  date.month = num;
  // parse day
  i++;
  num = 0;
  while(i < strlen(s)) {
    num = num * 10 + s[i] - '0';
    i++;
  }
  date.day = num;
  return date;
}

bool Date_t::is_valid_string_for_date(const char *s)
{
  // 格式正确性
  int i = 0;
  int dash_num = 0;
  while(i < strlen(s)) {
    if(s[i] >= '0' && s[i] <= '9') {
      i++;
    } else {
      if(s[i] == '-') {
        dash_num++;
        i++;
      } else {
        return false;
      }
    }
  }
  if(dash_num != 2 || s[strlen(s) - 1] < '0' || s[strlen(s) - 1] > '9') {
    return false;
  }
  // 日期正确性
  Date_t date = Date_t::parse_date_from_string(s);
  int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if((date.year % 4 == 0 && date.year % 100 != 0) || date.year % 400 == 0) {
    days_in_month[2]++;
  }
  if(date.year <= 0 || date.month <= 0 || date.day <= 0 || date.month > 12) {
    return false;
  }
  return date.day <= days_in_month[date.month];
}