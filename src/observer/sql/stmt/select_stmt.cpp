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
#include <sql/expr/expression_iterator.h>

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

  RC rc = RC::SUCCESS;
  BinderContext binder_context;

  // collect tables in `from` statement
  vector<Table *>                tables;
  unordered_map<string, Table *> table_map;
  
  vector<vector<Table *>>             relations_tables;
  vector<vector<SubQueryFilterStmt *>>  relations_sub_query_filter_stmts;
  vector<vector<SimpleFilterStmt *>>    relations_simple_filter_stmts;

  for (size_t i = 0; i < select_sql.relations.size(); i++) {
    // NOTE: std::vector<std::vector<std::pair<std::string, std::vector<ConditionSqlNode>>>> SelectSqlNode::relations
    BinderContext inner_binder_context;
    vector<Table *> inner_join_tables;
    unordered_map<string, Table *> inner_join_table_map;
    vector<SimpleFilterStmt *> inner_join_simple_filter_stmts;
    vector<SubQueryFilterStmt *> inner_join_sub_query_filter_stmts;

    for (size_t j = 0; j < select_sql.relations[i].size(); ++ j) {
      const char *table_name = select_sql.relations[i][j].first.c_str();
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
      inner_binder_context.add_table(table);
      tables.push_back(table);
      table_map.insert({table_name, table});

      inner_join_tables.push_back(table);
      inner_join_table_map.insert({table_name, table});

      if (j > 0) { // inner join
        // sikp the zeroth table because the conditions mean the join conditions between the j-th table and the former table
        // create filter statement for inner join
        // 之所以不和 where 一起处理，是因为 where 是先将所有表 join 再做 filter，会超时
        std::vector<ConditionSqlNode> inner_simple_conditions;
        std::vector<ConditionSqlNode> inner_sub_query_conditions;
        ExpressionBinder inner_expression_binder(inner_binder_context);
        for (auto &condition: select_sql.relations[i][j].second) {
          if(condition.left_type == ConditionSqlNode::SideType::Expr) {
            vector<unique_ptr<Expression>> bound;
            rc = inner_expression_binder.bind_expression(condition.left_expression, bound);
            if(OB_FAIL(rc)) {
              return rc;
            }
            condition.left_expression = std::move(bound.front());
          }
          if(condition.right_type == ConditionSqlNode::SideType::Expr) {
            vector<unique_ptr<Expression>> bound;
            rc = inner_expression_binder.bind_expression(condition.right_expression, bound);
            if(OB_FAIL(rc)) {
              return rc;
            }
            condition.right_expression = std::move(bound.front());
          }
          if(condition.left_type == ConditionSqlNode::SideType::SubQuery || condition.right_type == ConditionSqlNode::SideType::SubQuery) {
            inner_sub_query_conditions.emplace_back(std::move(condition));
          } else {
            inner_simple_conditions.emplace_back(std::move(condition));
          }
        }
        SimpleFilterStmt *simple_filter_stmt = nullptr;
        rc = SimpleFilterStmt::create(db, inner_join_tables[j - 1], &inner_join_table_map, inner_simple_conditions.data(), static_cast<int>(inner_simple_conditions.size()), simple_filter_stmt);
        if (OB_FAIL(rc)) {
          LOG_WARN("cannot construct filter stmt for inner join");
          return rc;
        }
        inner_join_simple_filter_stmts.push_back(simple_filter_stmt);
        SubQueryFilterStmt *sub_query_filter_stmt = nullptr;
        rc = SubQueryFilterStmt::create(db, inner_join_tables[j - 1], &inner_join_table_map, inner_sub_query_conditions.data(), static_cast<int>(inner_sub_query_conditions.size()), sub_query_filter_stmt);
        if(OB_FAIL(rc)) {
          LOG_WARN("cannot construct sub query filter stmt for inner join");
          return rc;
        }
        inner_join_sub_query_filter_stmts.push_back(sub_query_filter_stmt);
      }
    }

    relations_tables.push_back(std::move(inner_join_tables));
    relations_simple_filter_stmts.push_back(std::move(inner_join_simple_filter_stmts));
    relations_sub_query_filter_stmts.push_back(std::move(inner_join_sub_query_filter_stmts));
  }

  // collect query fields in `select` statement
  vector<unique_ptr<Expression>> bound_expressions;
  ExpressionBinder expression_binder(binder_context);
  
  for (unique_ptr<Expression> &expression : select_sql.expressions) {
    rc = expression_binder.bind_expression(expression, bound_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  vector<unique_ptr<Expression>> group_by_expressions;
  for (unique_ptr<Expression> &expression : select_sql.group_by) {
    rc = expression_binder.bind_expression(expression, group_by_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  vector<unique_ptr<Expression>> order_by_expressions;
  vector<OrderBySqlNode::OrderType> order_by_types;
  for(OrderBySqlNode &order_by_sql_node: select_sql.order_by) {
    rc = expression_binder.bind_expression(order_by_sql_node.expr, order_by_expressions);
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
      rc = expression_binder.bind_expression(condition.left_expression, bound);
      if(OB_FAIL(rc)) {
        return rc;
      }
      condition.left_expression = std::move(bound.front());
    }
    if(condition.right_type == ConditionSqlNode::SideType::Expr) {
      vector<unique_ptr<Expression>> bound;
      rc = expression_binder.bind_expression(condition.right_expression, bound);
      if(OB_FAIL(rc)) {
        return rc;
      }
      condition.right_expression = std::move(bound.front());
    }
    if(condition.left_type == ConditionSqlNode::SideType::SubQuery || condition.right_type == ConditionSqlNode::SideType::SubQuery) {
      sub_query_conditions.emplace_back(std::move(condition));
      // LOG_INFO("sub query");
    } else {
      // LOG_INFO("simple");
      simple_conditions.emplace_back(std::move(condition));
    }
  }

  // create simple filter statement in `where` statement
  SimpleFilterStmt *simple_filter_stmt = nullptr;
  rc = SimpleFilterStmt::create(db,
      default_table,
      &table_map,
      simple_conditions.data(),
      static_cast<int>(simple_conditions.size()),
      simple_filter_stmt);
  if (rc != RC::SUCCESS) {
    LOG_WARN("cannot construct filter stmt");
    return rc;
  }

  std::vector<FilterUnit<SimpleFilterObj> *> * filter_units = &simple_filter_stmt->filter_units();
  std::vector<int> filter_moved(filter_units->size(), 0); // 0 means not moved, 1 means moved
  
  for (size_t i = 0; i < select_sql.relations.size(); i++) {
    vector<Table *> inner_join_tables;
    unordered_map<string, Table *> inner_join_table_map;

    std::function<RC(std::unique_ptr<Expression>&)> can_move = [&](std::unique_ptr<Expression> &expr) -> RC {
      RC rc = RC::SUCCESS;
      if (expr->type() == ExprType::FIELD) {
        FieldExpr *fieldExpr = static_cast<FieldExpr *>(expr.get());
        if (inner_join_table_map.find(fieldExpr->field().table_name()) != inner_join_table_map.end()) {
          rc = RC::SUCCESS;
        }
        else rc = RC::INVALID_ARGUMENT;
      } else if (expr->type() == ExprType::VALUE) {
        rc = RC::SUCCESS;
      } else {
        rc = ExpressionIterator::iterate_child_expr(*expr, can_move);
      }
      return rc;
    };

    for (size_t j = 0; j < select_sql.relations[i].size(); ++ j) {
      inner_join_tables.push_back(relations_tables[i][j]);
      inner_join_table_map.insert({select_sql.relations[i][j].first, relations_tables[i][j]});
      if (j > 0) {
        for (size_t k = 0; k < filter_units->size(); ++ k) {
          // try to move the filter unit into former table's filter stmt
          if (filter_moved[k] == 1) {
            continue;
          }
          FilterUnit<SimpleFilterObj> *filter_unit = (*filter_units)[k];
          RC rc = can_move(filter_unit->left().expr);
          if (OB_FAIL(rc)) {
            continue;
          }
          rc = can_move(filter_unit->right().expr);
          if (OB_FAIL(rc)) {
            continue;
          }
          relations_simple_filter_stmts[i][j - 1]->push_back_filter_unit(filter_unit);
          LOG_DEBUG("move filter unit %d to table %d, table_name %s", k, j - 1, inner_join_tables[j]->name());
          filter_moved[k] = 1;
        }
      }
    }
  }

  for (size_t i = filter_moved.size(); i > 0; -- i) {
    if (filter_moved[i - 1] == 1) {
      filter_units->erase(filter_units->begin() + i - 1);
    }
  }
  LOG_DEBUG("filter units size %d", filter_units->size());

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
  select_stmt->relations_tables_.swap(relations_tables);
  select_stmt->relations_simple_filter_stmts_.swap(relations_simple_filter_stmts);
  select_stmt->relations_sub_query_filter_stmts_.swap(relations_sub_query_filter_stmts);
  stmt                      = select_stmt;
  return RC::SUCCESS;
}
