/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by WangYunlai on 2022/07/01.
//

#include "sql/operator/project_physical_operator.h"
#include "common/log/log.h"
#include "storage/record/record.h"
#include "storage/table/table.h"

using namespace std;

ProjectPhysicalOperator::ProjectPhysicalOperator(vector<unique_ptr<Expression>> &&expressions)
  : expressions_(std::move(expressions)), tuple_(expressions_)
{
}

RC ProjectPhysicalOperator::open(Trx *trx)
{
  if (children_.empty()) {
    return RC::SUCCESS;
  }

  PhysicalOperator *child = children_[0].get();
  RC                rc    = child->open(trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open child operator: %s", strrc(rc));
    return rc;
  }

  return RC::SUCCESS;
}

RC ProjectPhysicalOperator::next()
{
  if (children_.empty()) {
    return RC::RECORD_EOF;
  }
  return children_[0]->next();
}

RC ProjectPhysicalOperator::close()
{
  if (!children_.empty()) {
    children_[0]->close();
  }
  return RC::SUCCESS;
}
Tuple *ProjectPhysicalOperator::current_tuple()
{
  tuple_.set_tuple(children_[0]->current_tuple());
  return &tuple_;
}

#include <common/log/log.h>

RC ProjectPhysicalOperator::tuple_schema(TupleSchema &schema) const
{
  for (const unique_ptr<Expression> &expression : expressions_) {
    // LOG_DEBUG("expression name: %s", expression->name());
    if(expression->type() == ExprType::FIELD) {
      schema.append_cell(static_cast<FieldExpr *>(expression.get()));
    } 
    else if (expression->type() == ExprType::AGGREGATION) { // 避免认为聚合函数的字段名为空 而认为其是多表。同时也应该保有其表名
      if(dynamic_cast<const AggregateExpr *>(expression.get())->child()->type() == ExprType::FIELD) {
        // LOG_DEBUG("aggregate expression's table name: %s", dynamic_cast<const FieldExpr *>(dynamic_cast<const AggregateExpr *>(expression.get())->child().get())->table_name());
        schema.append_cell(static_cast<AggregateExpr *>(expression.get()));
      }
      else { // StarExpr
        schema.append_cell(expression->name());
      }
    }
    else if(expression->type() == ExprType::ARITHMETIC) {
      schema.append_cell(static_cast<ArithmeticExpr *>(expression.get()));
    } else {
      schema.append_cell(expression->name()); 
    }
  }
  return RC::SUCCESS;
}