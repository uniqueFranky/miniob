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
// Created by Wangyunlai on 2022/5/22.
//

#include "sql/stmt/update_stmt.h"
#include "storage/db/db.h"
#include "sql/parser/expression_binder.h"

UpdateStmt::UpdateStmt(Table *table, const std::string &attribute_name, Value *values, int value_amount, SimpleFilterStmt *filter_stmt)
    : table_(table), values_(values), value_amount_(value_amount), filter_stmt_(filter_stmt), attribute_name_(attribute_name)
{}

RC UpdateStmt::create(Db *db, UpdateSqlNode &update, Stmt *&stmt)
{
  // TODO
  Table *table = db->find_table(update.relation_name.c_str());
  if(table == nullptr) {
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }
  RC rc = RC::SUCCESS;
  std::unordered_map<std::string, Table *> table_map;
  table_map.insert_or_assign(update.relation_name, table);

  BinderContext binder_context;
  binder_context.add_table(table);
  ExpressionBinder expression_binder(binder_context);
  for(auto &condition: update.conditions) {
    if (condition.left_type == ConditionSqlNode::SideType::Expr) {
      vector<unique_ptr<Expression>> bound;
      rc = expression_binder.bind_expression(condition.left_expression, bound);
      if(OB_FAIL(rc)) {
        return rc;
      }
      condition.left_expression = std::move(bound.front());
    }
    if (condition.right_type == ConditionSqlNode::SideType::Expr) {
      vector<unique_ptr<Expression>> bound;
      rc = expression_binder.bind_expression(condition.right_expression, bound);
      if(OB_FAIL(rc)) {
        return rc;
      }
      condition.right_expression = std::move(bound.front());
    }
  }
  SimpleFilterStmt *filter_stmt = nullptr;
  rc = SimpleFilterStmt::create(db, table, &table_map, update.conditions.data(),
      static_cast<int>(update.conditions.size()), filter_stmt);
  if(OB_FAIL(rc)) {
    LOG_WARN("failed to create filter statement. rc=%d:%s", rc, strrc(rc));
    return RC::INTERNAL;
  }
  stmt = new UpdateStmt(table, update.attribute_name, const_cast<Value *>(&update.value), 1, filter_stmt);
  return RC::SUCCESS;
}

StmtType UpdateStmt::type() const
{
  return StmtType::UPDATE;
}