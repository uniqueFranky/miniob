//
// Created by 闫润邦 on 2024/11/19.
//

#include "sql/operator/sub_query_predicate_physical_operator.h"

static RC read_all_records(PhysicalOperator *child, std::vector<std::vector<Value>> &dest) {
  RC rc = RC::SUCCESS;
  while(OB_SUCC(child->next())) {
    Tuple *tuple = child->current_tuple();
    std::vector<Value> record;
    for(int i = 0; i < tuple->cell_num(); i++) {
      Value v;
      rc = tuple->cell_at(i, v);
      if(OB_FAIL(rc)) {
        return rc;
      }
      record.emplace_back(v);
    }
    dest.emplace_back(std::move(record));
  }
  return rc;
}


RC SubQueryPredicatePhysicalOperator::open(Trx *trx) {
  ASSERT(children_.size() == 2, "SubQueryPredicatePhysicalOperator must exactly have 2 children");

  RC rc = children_[0]->open(trx);
  if(OB_FAIL(rc)) {
    return rc;
  }
  rc = read_all_records(children_[0].get(), left_records_);
  if(OB_FAIL(rc)) {
    return rc;
  }
  children_[0]->close();

  rc = children_[1]->open(trx);
  if(OB_FAIL(rc)) {
    return rc;
  }
  rc = read_all_records(children_[1].get(), right_records_);
  if(OB_FAIL(rc)) {
    return rc;
  }
  children_[1]->close();

  TupleSchema schema;
  rc = tuple_schema(schema);
  if(OB_FAIL(rc)) {
    return rc;
  }
  for(int i = 0; i < schema.cell_num(); i++) {
    if(strcmp(field_.field_name(), schema.cell_at(i).field_name()) == 0 && strcmp(field_.table_name(), schema.cell_at(i).table_name()) == 0) {
      field_id_ = i;
      break;
    }
  }
  current_index_ = -1;

  for(int i = 0; i < schema.cell_num(); i++) {
    specs_.emplace_back(schema.cell_at(i));
  }


  // check for validation
  switch (comp_) {
    case CompOp::EQUAL_TO:
    case CompOp::NOT_EQUAL:
    case CompOp::LESS_THAN:
    case CompOp::LESS_EQUAL:
    case CompOp::GREAT_THAN:
    case CompOp::GREAT_EQUAL: {
      if(left_is_field_ && (right_records_.size() > 1 || (right_records_.size() > 0 && right_records_[0].size() != 1))) {
        return RC::INVALID_ARGUMENT;
      }
      if(!left_is_field_ && (left_records_.size() > 1 || (left_records_.size() > 0 && left_records_[0].size() != 1))) {
        return RC::INVALID_ARGUMENT;
      }
    } break;
    case CompOp::IN:
    case CompOp::NOT_IN: {
      if(left_is_field_ && (right_records_.size() > 0 && right_records_[0].size() != 1)) {
        return RC::INVALID_ARGUMENT;
      }
      if(!left_is_field_ && (left_records_.size() > 0 && left_records_[0].size() != 1)) {
        return RC::INVALID_ARGUMENT;
      }
    } break;
    default: {
      return RC::INVALID_ARGUMENT;
    }
  }

  return rc;
}

RC SubQueryPredicatePhysicalOperator::close() {
  return RC::SUCCESS;
}

RC SubQueryPredicatePhysicalOperator::tuple_schema(TupleSchema &schema) const {
  if(left_is_field_) {
    return children_[0]->tuple_schema(schema);
  } else {
    return children_[1]->tuple_schema(schema);
  }
}

// 暂不支持null的比较
static bool evaluate(const Value &v, const std::vector<std::vector<Value>> &set, CompOp comp, bool reverse) {
  switch(comp) {
    case CompOp::EQUAL_TO: {
      if(set.size() != 1 || set[0].size() != 1) {
        return false;
      }
      return v.compare(set[0][0]) == 0;
    } break;
    case CompOp::NOT_EQUAL: {
      if(set.size() != 1 || set[0].size() != 1) {
        return false;
      }
      return v.compare(set[0][0]) != 0;
    } break;
    case CompOp::GREAT_EQUAL: {
      if(set.size() != 1 || set[0].size() != 1) {
        return false;
      }
      if(!reverse) {
        return v.compare(set[0][0]) >= 0;
      } else {
        return set[0][0].compare(v) >= 0;
      }
    } break;

    case CompOp::GREAT_THAN: {
      if(set.size() != 1 || set[0].size() != 1) {
        return false;
      }
      if(!reverse) {
        return v.compare(set[0][0]) > 0;
      } else {
        return set[0][0].compare(v) > 0;
      }
    } break;
    case CompOp::LESS_EQUAL: {
      if(set.size() != 1 || set[0].size() != 1) {
        return false;
      }
      if(!reverse) {
        return v.compare(set[0][0]) <= 0;
      } else {
        return set[0][0].compare(v) <= 0;
      }
    } break;
    case CompOp::LESS_THAN: {
      if(set.size() != 1 || set[0].size() != 1) {
        return false;
      }
      if(!reverse) {
        return v.compare(set[0][0]) < 0;
      } else {
        return set[0][0].compare(v) < 0;
      }
    } break;
    case CompOp::IN: {
      if(set.size() < 1 || set[0].size() != 1 || reverse) {
        return false;
      }
      for(const std::vector<Value> &s: set) {
        if(v.compare(s[0]) == 0) {
          return true;
        }
      }
      return false;
    } break;
    case CompOp::NOT_IN: {
      if((set.size() > 0 && set[0].size() != 1) || reverse) {
        return false;
      }
      for(const std::vector<Value> &s: set) {
        if(v.compare(s[0]) == 0) {
          return false;
        }
      }
      return true;
    } break;
    default: {
      LOG_ERROR("unsupported comp op");
      return false;
    }
  }
}

RC SubQueryPredicatePhysicalOperator::next() {
  if(left_is_field_) {
    while(++current_index_ < static_cast<int>(left_records_.size())) {
      const std::vector<Value> &record = left_records_[current_index_];
      if(evaluate(record[field_id_], right_records_, comp_, false)) {
        return RC::SUCCESS;
      }
    }
  } else {
    while(++current_index_ < static_cast<int>(right_records_.size())) {
      const std::vector<Value> &record = right_records_[current_index_];
      if(evaluate(record[field_id_], left_records_, comp_, true)) {
        return RC::SUCCESS;
      }
    }
  }
  return RC::RECORD_EOF;
}

Tuple *SubQueryPredicatePhysicalOperator::current_tuple() {
  const std::vector<Value> *record;
  if(left_is_field_) {
    record = &left_records_[current_index_];
  } else {
    record = &right_records_[current_index_];
  }
  ValueListTuple *tuple = new ValueListTuple();

  tuple->set_names(specs_);
  tuple->set_cells(*record);
  return tuple;
}