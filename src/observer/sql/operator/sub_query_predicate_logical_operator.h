//
// Created by 闫润邦 on 2024/11/19.
//

#ifndef MINIDB_SUB_QUERY_PREDICATE_LOGICAL_OPERATOR_H
#define MINIDB_SUB_QUERY_PREDICATE_LOGICAL_OPERATOR_H

#include "sql/operator/logical_operator.h"

// 两个child，一个child是下层算子返回的tuple，另一个child是条件中的sub_query
class SubQueryPredicateLogicalOperator: public LogicalOperator {
public:
  SubQueryPredicateLogicalOperator(CompOp op): comp_(op) {};
  void set_field(bool is_left, const Field &field) { left_is_field_ = is_left; field_ = field; }

public:
  virtual LogicalOperatorType type() const override { return LogicalOperatorType::SUB_QUERY_PREDICATE; }
  CompOp comp_op() const { return comp_; }

  bool left_is_field() const { return left_is_field_; }
  bool right_is_field() const { return !left_is_field_; }
  Field field() const { return field_; }

private:
  CompOp comp_;
  Field field_;
  bool left_is_field_;
};


#endif  // MINIDB_SUB_QUERY_PREDICATE_LOGICAL_OPERATOR_H
