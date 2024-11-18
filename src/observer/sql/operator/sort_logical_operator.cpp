//
// Created by 闫润邦 on 2024/11/18.
//

#include "sql/operator/sort_logical_operator.h"

void SortLogicalOperator::add_sort_metric(std::unique_ptr<Expression> field, OrderBySqlNode::OrderType sort_type) {
  expressions_.emplace_back(std::move(field));
  sort_types_.emplace_back(sort_type);
}

std::vector<OrderBySqlNode::OrderType> &SortLogicalOperator::sort_types() {
  return sort_types_;
}