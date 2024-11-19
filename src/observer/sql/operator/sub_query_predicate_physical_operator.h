//
// Created by 闫润邦 on 2024/11/19.
//
#include "sql/operator/physical_operator.h"

class SubQueryPredicatePhysicalOperator: public PhysicalOperator {
public:
  SubQueryPredicatePhysicalOperator() = default;
  virtual ~SubQueryPredicatePhysicalOperator() = default;

  virtual PhysicalOperatorType type() const override { return PhysicalOperatorType::SUB_QUERY_PREDICATE; }

  virtual RC open(Trx *trx) override;
  virtual RC close() override;
  virtual RC next() override;

  virtual Tuple *current_tuple() override;

  virtual RC tuple_schema(TupleSchema &schema) const override;

public:
  void set_comp(CompOp comp) { comp_ = comp; }
  void set_field(bool is_left, const Field &field) { left_is_field_ = is_left; field_ = field; }

private:
  std::vector<std::vector<Value>> left_records_;
  std::vector<std::vector<Value>> right_records_;
  CompOp comp_;
  bool left_is_field_;
  Field field_;
  ssize_t field_id_;
  int current_index_;
  std::vector<TupleCellSpec> specs_;
};