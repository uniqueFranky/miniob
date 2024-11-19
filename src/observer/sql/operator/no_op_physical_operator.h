//
// Created by 闫润邦 on 2024/11/19.
//

#ifndef MINIDB_NO_OP_PHYSICAL_OPERATOR_H
#define MINIDB_NO_OP_PHYSICAL_OPERATOR_H
#include "sql/operator/physical_operator.h"
class NoOpPhysicalOperator: public PhysicalOperator {
public:
  NoOpPhysicalOperator() = default;
  virtual RC open(Trx *trx) override { return RC::SUCCESS; }
  virtual RC next() override { return RC::RECORD_EOF; }
  virtual RC close() override { return RC::SUCCESS; }
  virtual Tuple *current_tuple() override { return nullptr; }
  virtual PhysicalOperatorType type() const override { return PhysicalOperatorType::NO_OP; }
};

#endif  // MINIDB_NO_OP_PHYSICAL_OPERATOR_H
