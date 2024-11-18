//
// Created by 闫润邦 on 2024/11/18.
//

#include "sort_physical_operator.h"

RC SortPhysicalOperator::open(Trx *trx) {
  ASSERT(children_.size() == 1, "Sort Physical Operator only supports one child.");
  // children_.front()是一个project operator
  std::unique_ptr<PhysicalOperator> child = std::move(children_.front());
  ASSERT(child->type() == PhysicalOperatorType::PROJECT, "Child of Sort Physical Operator must be Project.");
  RC rc = child->open(trx);
  if(OB_FAIL(rc)) {
    return rc;
  }
  while(OB_SUCC(child->next())) {
    child->current_tuple()->
  }
}
virtual RC next() override;
virtual RC close() override;

virtual Tuple *current_tuple() override;

virtual RC tuple_schema(TupleSchema &schema) const override;