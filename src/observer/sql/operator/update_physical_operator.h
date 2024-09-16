//
// Created by 闫润邦 on 2024/9/16.
//

#ifndef MINIDB_UPDATE_PHYSICAL_OPERATOR_H
#define MINIDB_UPDATE_PHYSICAL_OPERATOR_H

#include "sql/operator/physical_operator.h"

class UpdatePhysicalOperator: public PhysicalOperator {
public:
  UpdatePhysicalOperator(Table *table, const std::string &attribute_name, Value *values, int value_amount): table_(table), attribute_name_(attribute_name), values_(values), value_amount_(value_amount) {}
  virtual ~UpdatePhysicalOperator() = default;

  PhysicalOperatorType type() const override { return PhysicalOperatorType::UPDATE; }

  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;

  Tuple *current_tuple() override { return nullptr; }

private:
  Table *table_ = nullptr;
  std::string attribute_name_;
  Value *values_ = nullptr;
  int value_amount_ = 0;
};

#endif  // MINIDB_UPDATE_PHYSICAL_OPERATOR_H
