/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2024/05/29.
//

#include "sql/expr/aggregator.h"
#include "common/log/log.h"

RC SumAggregator::accumulate(const Value &value)
{
  if (value_.attr_type() == AttrType::UNDEFINED) {
    value_ = value;
    return RC::SUCCESS;
  }
  if(value.is_null()) {
    return RC::SUCCESS;
  }
  ASSERT(value.attr_type() == value_.attr_type(), "type mismatch. value type: %s, value_.type: %s", 
        attr_type_to_string(value.attr_type()), attr_type_to_string(value_.attr_type()));
  
  Value::add(value, value_, value_);
  return RC::SUCCESS;
}

RC SumAggregator::evaluate(Value& result)
{
  result = value_;
  return RC::SUCCESS;
}


RC CountAggregator::accumulate(const Value &value)
{
  if(value.is_null()) {
    return RC::SUCCESS;
  }
  RC rc = Value::add(Value(1), value_, value_);
  return rc;
}

RC CountAggregator::evaluate(Value &result)
{
  result = value_;
  return RC::SUCCESS;
}

RC AvgAggregator::accumulate(const Value &value)
{
  if(value.is_null()) {
    return RC::SUCCESS;
  }
  RC rc = sum_aggregator_->accumulate(value);
  if(OB_FAIL(rc)) {
    return rc;
  }
  rc = count_aggregator_->accumulate(value);
  return rc;
}

RC AvgAggregator::evaluate(Value &result)
{
  Value count;
  Value sum;
  RC rc = count_aggregator_->evaluate(count);
  if(OB_FAIL(rc)) {
    return rc;
  }
  rc = sum_aggregator_->evaluate(sum);
  if(OB_FAIL(rc)) {
    return rc;
  }
  result.set_type(AttrType::FLOATS);
  if(count.is_null() || count.get_int() == 0) {
    result = Value();
  } else {
    rc = Value::divide(sum, count, result);
  }
  return rc;
}

RC MaxAggregator::accumulate(const Value &value)
{
  if(value.is_null()) {
    return RC::SUCCESS;
  }
  if(value_.attr_type() == AttrType::UNDEFINED) {
    value_ = value;
  } else {
    if(value.compare(value_) > 0) {
      value_ = value;
    }
  }
  return RC::SUCCESS;
}

RC MaxAggregator::evaluate(Value &result)
{
  result = value_;
  return RC::SUCCESS;
}

RC MinAggregator::accumulate(const Value &value)
{
  if(value.is_null()) {
    return RC::SUCCESS;
  }
  if(value_.attr_type() == AttrType::UNDEFINED) {
    value_ = value;
  } else {
    if(value.compare(value_) < 0) {
      value_ = value;
    }
  }
  return RC::SUCCESS;
}

RC MinAggregator::evaluate(Value &result)
{
  result = value_;
  return RC::SUCCESS;
}