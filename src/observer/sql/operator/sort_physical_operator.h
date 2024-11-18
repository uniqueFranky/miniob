//
// Created by 闫润邦 on 2024/11/18.
//

#ifndef MINIDB_SORT_PHYSICAL_OPERATOR_H
#define MINIDB_SORT_PHYSICAL_OPERATOR_H

#endif  // MINIDB_SORT_PHYSICAL_OPERATOR_H

#include "common/rc.h"
#include "sql/operator/physical_operator.h"

class SortPhysicalOperator: public PhysicalOperator {
public:
  SortPhysicalOperator(std::vector<std::unique_ptr<Expression>> &sort_fields, std::vector<OrderBySqlNode::OrderType> &sort_types): sort_fields_(std::move(sort_fields)), sort_types_(std::move(sort_types)) {}
  ~SortPhysicalOperator() = default;

  virtual RC open(Trx *trx) override;
  virtual RC next() override;
  virtual RC close() override;

  virtual PhysicalOperatorType type() const override { return PhysicalOperatorType::SORT; }

  virtual Tuple *current_tuple() override;

  virtual RC tuple_schema(TupleSchema &schema) const override;

private:
  std::vector<std::unique_ptr<Expression>> sort_fields_;
  std::vector<OrderBySqlNode::OrderType> sort_types_;
  std::vector<ssize_t> sort_ids_;
  std::vector<std::vector<Value>> records_;
  int current_index;
  std::vector<TupleCellSpec> tuple_cell_specs_;
};