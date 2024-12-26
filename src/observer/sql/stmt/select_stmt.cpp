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
// Created by Wangyunlai on 2022/6/6.
//

#include "sql/stmt/select_stmt.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "sql/stmt/filter_stmt.h"
#include "storage/db/db.h"
#include "storage/table/table.h"
#include "sql/parser/expression_binder.h"

using namespace std;
using namespace common;

SelectStmt::~SelectStmt()
{
  if (nullptr != simple_filter_stmt_) {
    delete simple_filter_stmt_;
    simple_filter_stmt_ = nullptr;
  }
}

RC SelectStmt::create(Db *db, SelectSqlNode &select_sql, Stmt *&stmt)
{
  if (nullptr == db) {
    LOG_WARN("invalid argument. db is null");
    return RC::INVALID_ARGUMENT;
  }

  BinderContext binder_context;

  // collect tables in `from` statement
  vector<Table *>                tables;
  unordered_map<string, Table *> table_map;
  for (size_t i = 0; i < select_sql.relations.size(); i++) {
    const char *table_name = select_sql.relations[i].c_str();
    if (nullptr == table_name) {
      LOG_WARN("invalid argument. relation name is null. index=%d", i);
      return RC::INVALID_ARGUMENT;
    }

    Table *table = db->find_table(table_name);
    if (nullptr == table) {
      LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
      return RC::SCHEMA_TABLE_NOT_EXIST;
    }

    binder_context.add_table(table);
    tables.push_back(table);
    table_map.insert({table_name, table});
  }

  // collect query fields in `select` statement
  vector<unique_ptr<Expression>> bound_expressions;
  ExpressionBinder expression_binder(binder_context);
  
  for (unique_ptr<Expression> &expression : select_sql.expressions) {
    RC rc = expression_binder.bind_expression(expression, bound_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  vector<unique_ptr<Expression>> group_by_expressions;
  for (unique_ptr<Expression> &expression : select_sql.group_by) {
    RC rc = expression_binder.bind_expression(expression, group_by_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  vector<unique_ptr<Expression>> order_by_expressions;
  vector<OrderBySqlNode::OrderType> order_by_types;
  for(OrderBySqlNode &order_by_sql_node: select_sql.order_by) {
    RC rc = expression_binder.bind_expression(order_by_sql_node.expr, order_by_expressions);
    order_by_types.emplace_back(order_by_sql_node.order_type);
    if(OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  Table *default_table = nullptr;
  if (tables.size() == 1) {
    default_table = tables[0];
  }


  std::vector<ConditionSqlNode> simple_conditions;
  std::vector<ConditionSqlNode> sub_query_conditions;
  for(auto &condition: select_sql.conditions) {
    if(condition.left_type == ConditionSqlNode::SideType::Expr) {
      vector<unique_ptr<Expression>> bound;
      expression_binder.bind_expression(condition.left_expression, bound);
      condition.left_expression = std::move(bound.front());
    }
    if(condition.right_type == ConditionSqlNode::SideType::Expr) {
      vector<unique_ptr<Expression>> bound;
      expression_binder.bind_expression(condition.right_expression, bound);
      condition.right_expression = std::move(bound.front());
    }
    if(condition.left_type == ConditionSqlNode::SideType::SubQuery || condition.right_type == ConditionSqlNode::SideType::SubQuery) {
      sub_query_conditions.emplace_back(std::move(condition));
      LOG_INFO("sub query");
    } else {
      LOG_INFO("simple");
      simple_conditions.emplace_back(std::move(condition));
    }
  }
  // create simple filter statement in `where` statement
  SimpleFilterStmt *simple_filter_stmt = nullptr;
  RC          rc          = SimpleFilterStmt::create(db,
      default_table,
      &table_map,
      simple_conditions.data(),
      static_cast<int>(simple_conditions.size()),
      simple_filter_stmt);
  if (rc != RC::SUCCESS) {
    LOG_WARN("cannot construct filter stmt");
    return rc;
  }

  // create sub query filter statement in `where` statement
  SubQueryFilterStmt *sub_query_filter_stmt = nullptr;
  rc = SubQueryFilterStmt::create(db, default_table, &table_map, sub_query_conditions.data(), static_cast<int>(sub_query_conditions.size()), sub_query_filter_stmt);
  if(OB_FAIL(rc)) {
    return rc;
  }

  // everything alright
  SelectStmt *select_stmt = new SelectStmt();

  select_stmt->tables_.swap(tables);
  select_stmt->query_expressions_.swap(bound_expressions);
  select_stmt->simple_filter_stmt_ = simple_filter_stmt;
  select_stmt->sub_query_filter_stmt_ = sub_query_filter_stmt;
  select_stmt->group_by_.swap(group_by_expressions);
  select_stmt->order_by_.swap(order_by_expressions);
  select_stmt->order_by_type_.swap(order_by_types);
  stmt                      = select_stmt;
  return RC::SUCCESS;
}
