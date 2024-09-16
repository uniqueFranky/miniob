//
// Created by 闫润邦 on 2024/9/15.
//

#ifndef MINIDB_UPDATE_LOGICAL_OPERATOR_H
#define MINIDB_UPDATE_LOGICAL_OPERATOR_H

#include "sql/operator/logical_operator.h"
class UpdateLogicalOperator: public LogicalOperator {
public:
  UpdateLogicalOperator(Table *table, const std::string &attribute_name, Value *values, int value_amount): table_(table), attribute_name_(attribute_name), values_(values), value_amount_(value_amount) {};
  virtual ~UpdateLogicalOperator() = default;

  LogicalOperatorType type() const override { return LogicalOperatorType::UPDATE; }

  Table *table() { return table_; }
  std::string attribute_name() const { return attribute_name_; }
  Value *values() { return values_; }
  int value_amount() const { return value_amount_; }

private:
  Table *table_ = nullptr;
  std::string attribute_name_;
  Value *values_ = nullptr;
  int value_amount_ = 0;
};

#endif  // MINIDB_UPDATE_LOGICAL_OPERATOR_H
