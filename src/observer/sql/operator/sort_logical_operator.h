//
// Created by 闫润邦 on 2024/11/18.
//

#ifndef MINIDB_SORT_LOGICAL_OPERATOR_H
#define MINIDB_SORT_LOGICAL_OPERATOR_H

#include "sql/operator/logical_operator.h"

class SortLogicalOperator: public LogicalOperator {
public:
  SortLogicalOperator() = default;
  ~SortLogicalOperator() = default;
  LogicalOperatorType type() const override { return LogicalOperatorType::SORT; }

public:
  void add_sort_metric(std::unique_ptr<Expression> field, OrderBySqlNode::OrderType sort_type);
  std::vector<OrderBySqlNode::OrderType> &sort_types();

private:
  std::vector<OrderBySqlNode::OrderType> sort_types_;
};

#endif  // MINIDB_SORT_LOGICAL_OPERATOR_H
