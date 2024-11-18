//
// Created by 闫润邦 on 2024/11/18.
//

#include "sort_physical_operator.h"
#include <algorithm>

RC SortPhysicalOperator::open(Trx *trx) {
  ASSERT(children_.size() == 1, "Sort Physical Operator only supports one child.");
  // children_.front()是一个project operator
  PhysicalOperator *child = children_.front().get();
  ASSERT(child->type() == PhysicalOperatorType::PROJECT, "Child of Sort Physical Operator must be Project.");
  RC rc = child->open(trx);
  if(OB_FAIL(rc)) {
    return rc;
  }
  while(OB_SUCC(child->next())) {
    std::vector<Value> record;
    Tuple *tuple = child->current_tuple();
    for(int i = 0; i < tuple->cell_num(); i++) {
      Value v;
      rc = tuple->cell_at(i, v);
      if(OB_FAIL(rc)) {
        return rc;
      }
      record.emplace_back(std::move(v));
    }
    records_.emplace_back(std::move(record));
  }

  // 计算关键字是record数组里的第几个
  TupleSchema schema;
  child->tuple_schema(schema);
  for(int i = 0; i < sort_fields_.size(); i++) {
    FieldExpr *field = dynamic_cast<FieldExpr *>(sort_fields_[i].get());
    for(int j = 0; j < schema.cell_num(); j++) {
      if(strcmp(schema.cell_at(j).table_name(), field->table_name()) == 0 && strcmp(schema.cell_at(j).field_name(), field->field_name()) == 0) {
        sort_ids_.emplace_back(j);
        break;
      }
    }
  }

  std::sort(records_.begin(), records_.end(), [this](const std::vector<Value> &v1, const std::vector<Value> &v2) {
    for(int i = 0; i < sort_fields_.size(); i++) {
      OrderBySqlNode::OrderType type = sort_types_[i];
      ssize_t id = sort_ids_[i];
      int result = v1[id].compare(v2[id]);
      if(result == 0) {
        continue;
      } else if(result < 0) {
        return type == OrderBySqlNode::OrderType::ASC;
      } else {
        return type == OrderBySqlNode::OrderType::DESC;
      }
    }
    return false;
  });

  current_index = -1;

  for(int i = 0; i < schema.cell_num(); i++) {
    tuple_cell_specs_.emplace_back(schema.cell_at(i));
  }

  return rc;
}
RC SortPhysicalOperator::next() {
  current_index++;
  if(current_index >= records_.size()) {
    return RC::RECORD_EOF;
  }
  return RC::SUCCESS;
}

RC SortPhysicalOperator::close() {
  return children_[0]->close();
}

Tuple *SortPhysicalOperator::current_tuple() {
  ValueListTuple *tuple = new ValueListTuple();
  TupleSchema schema;
  tuple_schema(schema);
  tuple->set_names(tuple_cell_specs_);
  tuple->set_cells(records_[current_index]);
  return tuple;
}

RC SortPhysicalOperator::tuple_schema(TupleSchema &schema) const {
  return children_.front()->tuple_schema(schema);
}