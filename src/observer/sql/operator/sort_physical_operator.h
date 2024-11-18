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
  SortPhysicalOperator() = default;
  ~SortPhysicalOperator() = default;

  virtual RC open(Trx *trx) override;
  virtual RC next() override;
  virtual RC close() override;

  virtual Tuple *current_tuple() override;

  virtual RC tuple_schema(TupleSchema &schema) const override;

private:
  std::vector<RowTuple> tuples_;
};