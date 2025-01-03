/* Copyright (c) 2023 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2023/08/16.
//

#include "sql/optimizer/logical_plan_generator.h"

#include <common/log/log.h>

#include "sql/operator/calc_logical_operator.h"
#include "sql/operator/delete_logical_operator.h"
#include "sql/operator/explain_logical_operator.h"
#include "sql/operator/insert_logical_operator.h"
#include "sql/operator/join_logical_operator.h"
#include "sql/operator/logical_operator.h"
#include "sql/operator/predicate_logical_operator.h"
#include "sql/operator/project_logical_operator.h"
#include "sql/operator/table_get_logical_operator.h"
#include "sql/operator/group_by_logical_operator.h"
#include "sql/operator/update_logical_operator.h"
#include "sql/operator/sort_logical_operator.h"
#include "sql/operator/sub_query_predicate_logical_operator.h"

#include "sql/stmt/calc_stmt.h"
#include "sql/stmt/delete_stmt.h"
#include "sql/stmt/explain_stmt.h"
#include "sql/stmt/filter_stmt.h"
#include "sql/stmt/insert_stmt.h"
#include "sql/stmt/select_stmt.h"
#include "sql/stmt/update_stmt.h"
#include "sql/stmt/stmt.h"

#include "sql/expr/expression_iterator.h"

using namespace std;
using namespace common;

RC LogicalPlanGenerator::create(Stmt *stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  RC rc = RC::SUCCESS;
  switch (stmt->type()) {
    case StmtType::CALC: {
      CalcStmt *calc_stmt = static_cast<CalcStmt *>(stmt);

      rc = create_plan(calc_stmt, logical_operator);
    } break;

    case StmtType::SELECT: {
      SelectStmt *select_stmt = static_cast<SelectStmt *>(stmt);

      rc = create_plan(select_stmt, logical_operator);
    } break;

    case StmtType::INSERT: {
      InsertStmt *insert_stmt = static_cast<InsertStmt *>(stmt);

      rc = create_plan(insert_stmt, logical_operator);
    } break;

    case StmtType::DELETE: {
      DeleteStmt *delete_stmt = static_cast<DeleteStmt *>(stmt);

      rc = create_plan(delete_stmt, logical_operator);
    } break;

    case StmtType::EXPLAIN: {
      ExplainStmt *explain_stmt = static_cast<ExplainStmt *>(stmt);

      rc = create_plan(explain_stmt, logical_operator);
    } break;

    case StmtType::UPDATE: {
      UpdateStmt *update_stmt = static_cast<UpdateStmt *>(stmt);

      rc = create_plan(update_stmt, logical_operator);
    } break;

    default: {
      rc = RC::UNIMPLEMENTED;
    }
  }
  return rc;
}

RC LogicalPlanGenerator::create_plan(CalcStmt *calc_stmt, std::unique_ptr<LogicalOperator> &logical_operator)
{
  logical_operator.reset(new CalcLogicalOperator(std::move(calc_stmt->expressions())));
  return RC::SUCCESS;
}

RC LogicalPlanGenerator::create_plan(SelectStmt *select_stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  unique_ptr<LogicalOperator> *last_oper = nullptr;

  unique_ptr<LogicalOperator> table_oper(nullptr);
  last_oper = &table_oper;

  // const std::vector<Table *> &tables = select_stmt->tables();
  // for (Table *table : tables) {

  //   unique_ptr<LogicalOperator> table_get_oper(new TableGetLogicalOperator(table, ReadWriteMode::READ_ONLY));
  //   if (table_oper == nullptr) {
  //     table_oper = std::move(table_get_oper);
  //   } else {
  //     JoinLogicalOperator *join_oper = new JoinLogicalOperator;
  //     join_oper->add_child(std::move(table_oper));
  //     join_oper->add_child(std::move(table_get_oper));
  //     table_oper = unique_ptr<LogicalOperator>(join_oper);
  //   }
  // }
  const std::vector<std::vector<Table *>> &relations_tables = select_stmt->relations_tables();
  const std::vector<std::vector<SimpleFilterStmt *>> &relations_simple_filter_stmts = select_stmt->relations_simple_filter_stmts();
  const std::vector<std::vector<SubQueryFilterStmt *>> &relations_sub_query_filter_stmts = select_stmt->relations_sub_query_filter_stmts();

  for (size_t i = 0; i < relations_tables.size(); i++) {
    unique_ptr<LogicalOperator> inner_join_table_oper(nullptr);
    // LOG_DEBUG("The %d-th relations_tables size: %d", i, relations_tables[0].size());
    for (size_t j = 0; j < relations_tables[i].size(); j++) {
      unique_ptr<LogicalOperator> table_get_oper(new TableGetLogicalOperator(relations_tables[i][j], ReadWriteMode::READ_ONLY));
      if (inner_join_table_oper == nullptr) {
        inner_join_table_oper = std::move(table_get_oper);
      } 
      else { // inner join relations 只有一个表的时候不会进入这个分支
        JoinLogicalOperator* join_oper = new JoinLogicalOperator;
        join_oper->add_child(std::move(inner_join_table_oper));
        join_oper->add_child(std::move(table_get_oper));
        inner_join_table_oper = unique_ptr<LogicalOperator>(join_oper);
        unique_ptr<LogicalOperator> inner_join_predicates_oper;
        // LOG_DEBUG("The %d-th relations_tables[%d] join", i, j);
        RC rc = create_plan(relations_simple_filter_stmts[i][j - 1], inner_join_predicates_oper); // j - 1 是因为 filter 都是和前一个表的关系
        if (OB_FAIL(rc)) {  
          LOG_WARN("failed to create inner join predicates logical plan. rc=%s", strrc(rc));
          return rc;
        }
        if (inner_join_predicates_oper) {
          LOG_DEBUG("The %d-th relations_tables[%d] inner join predicate, filter_units size = %d", i, j, relations_simple_filter_stmts[i][j - 1]->filter_units().size());
          inner_join_predicates_oper->add_child(std::move(inner_join_table_oper));
          inner_join_table_oper = std::move(inner_join_predicates_oper);
        }
        unique_ptr<LogicalOperator> inner_join_sub_query_oper;
        rc = create_plan(relations_sub_query_filter_stmts[i][j - 1], inner_join_table_oper, inner_join_sub_query_oper);
        if (OB_FAIL(rc)) {
          LOG_WARN("failed to create inner join sub query logical plan. rc=%s", strrc(rc));
          return rc;
        }
        if (inner_join_sub_query_oper) {
          // LOG_DEBUG("The %d-th relations_tables[%d] inner join sub query", i, j);
          // we don't need to add the inner_join_table_oper to inner_join_sub_query_oper here, because it has been added in the create_plan function
          // inner_join_sub_query_oper->add_child(std::move(inner_join_table_oper));
          inner_join_table_oper = std::move(inner_join_sub_query_oper);
        }
      }
    }
    if (table_oper == nullptr) {
      // LOG_DEBUG("The %d-th relations_tables is the first table", i);
      table_oper = std::move(inner_join_table_oper);
    } else {
      // LOG_DEBUG("The %d-th relations_tables is not the first table", i);
      JoinLogicalOperator *join_oper = new JoinLogicalOperator;
      join_oper->add_child(std::move(table_oper));
      join_oper->add_child(std::move(inner_join_table_oper));
      table_oper = unique_ptr<LogicalOperator>(join_oper);
    }
  }

  unique_ptr<LogicalOperator> predicate_oper;

  RC rc = create_plan(select_stmt->simple_filter_stmt(), predicate_oper);
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to create predicate logical plan. rc=%s", strrc(rc));
    return rc;
  }

  if (predicate_oper) {
    if (*last_oper) {
      predicate_oper->add_child(std::move(*last_oper));
    }
    // LOG_DEBUG("The predicate_oper is not null");
    last_oper = &predicate_oper;
  }

  unique_ptr<LogicalOperator> sub_query_predicate_oper;
  rc = create_plan(select_stmt->sub_query_filter_stmt(), *last_oper, sub_query_predicate_oper);
  if(OB_FAIL(rc)) {
    return rc;
  }
  last_oper = &sub_query_predicate_oper;

  unique_ptr<LogicalOperator> group_by_oper;
  rc = create_group_by_plan(select_stmt, group_by_oper);
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to create group by logical plan. rc=%s", strrc(rc));
    return rc;
  }

  if (group_by_oper) {
    if (*last_oper) {
      group_by_oper->add_child(std::move(*last_oper));
    }

    last_oper = &group_by_oper;
  }

  auto project_oper = make_unique<ProjectLogicalOperator>(std::move(select_stmt->query_expressions()));
  if (*last_oper) {
    project_oper->add_child(std::move(*last_oper));
  }

  // Order By
  unique_ptr<SortLogicalOperator> sort_oper = make_unique<SortLogicalOperator>();
  std::vector<std::unique_ptr<Expression>> order_by_exprs = std::move(select_stmt->order_by());
  std::vector<OrderBySqlNode::OrderType> order_by_types = std::move(select_stmt->order_by_type());
  assert(order_by_exprs.size() == order_by_types.size());
  for(int i = 0; i < order_by_exprs.size(); i++) {
    sort_oper->add_sort_metric(std::move(order_by_exprs[i]), order_by_types[i]);
  }
  sort_oper->add_child(std::move(project_oper));
  logical_operator = std::move(sort_oper);
  return RC::SUCCESS;
}

RC LogicalPlanGenerator::create_plan(SimpleFilterStmt *filter_stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  RC                                  rc = RC::SUCCESS;
  std::vector<unique_ptr<Expression>> cmp_exprs;
  std::vector<FilterUnit<SimpleFilterObj> *>    &filter_units = filter_stmt->filter_units();
  for (FilterUnit<SimpleFilterObj> *&filter_unit : filter_units) {

    unique_ptr<Expression> left = std::move(filter_unit->left().expr);
    unique_ptr<Expression> right = std::move(filter_unit->right().expr);

    // null value type setting
    if(left->type() == ExprType::VALUE) {
      ValueExpr *ve = static_cast<ValueExpr *>(left.get());
      Value v = ve->get_value();
      if(v.is_null()) {
        v.set_type(right->value_type());
        left = std::make_unique<ValueExpr>(v);
      }
    }
    if(right->type() == ExprType::VALUE) {
      LOG_INFO("right is a value");
      ValueExpr *ve = static_cast<ValueExpr *>(right.get());
      Value v = ve->get_value();
      if(v.is_null()) {
        v.set_type((left->value_type()));
        right = std::make_unique<ValueExpr>(v);
      }
    }

    if (left->value_type() != right->value_type()) {
      auto left_to_right_cost = implicit_cast_cost(left->value_type(), right->value_type());
      auto right_to_left_cost = implicit_cast_cost(right->value_type(), left->value_type());
      if (left_to_right_cost <= right_to_left_cost && left_to_right_cost != INT32_MAX) {
        ExprType left_type = left->type();
        auto cast_expr = make_unique<CastExpr>(std::move(left), right->value_type());
        if (left_type == ExprType::VALUE) {
          Value left_val;
          if (OB_FAIL(rc = cast_expr->try_get_value(left_val)))
          {
            LOG_WARN("failed to get value from left child", strrc(rc));
            return rc;
          }
          left = make_unique<ValueExpr>(left_val);
        } else {
          left = std::move(cast_expr);
        }
      } else if (right_to_left_cost < left_to_right_cost && right_to_left_cost != INT32_MAX) {
        ExprType right_type = right->type();
        auto cast_expr = make_unique<CastExpr>(std::move(right), left->value_type());
        if (right_type == ExprType::VALUE) {
          Value right_val;
          if (OB_FAIL(rc = cast_expr->try_get_value(right_val)))
          {
            LOG_WARN("failed to get value from right child", strrc(rc));
            return rc;
          }
          right = make_unique<ValueExpr>(right_val);
        } else {
          right = std::move(cast_expr);
        }

      } else {
        rc = RC::UNSUPPORTED;
        LOG_WARN("unsupported cast from %s to %s", attr_type_to_string(left->value_type()), attr_type_to_string(right->value_type()));
        return rc;
      }
    }

    ComparisonExpr *cmp_expr = new ComparisonExpr(filter_unit->comp(), std::move(left), std::move(right));
    cmp_exprs.emplace_back(cmp_expr);
  }

  unique_ptr<PredicateLogicalOperator> predicate_oper;
  if (!cmp_exprs.empty()) {
    unique_ptr<ConjunctionExpr> conjunction_expr(new ConjunctionExpr(ConjunctionExpr::Type::AND, cmp_exprs));
    predicate_oper = unique_ptr<PredicateLogicalOperator>(new PredicateLogicalOperator(std::move(conjunction_expr)));
  }

  logical_operator = std::move(predicate_oper);
  return rc;
}

RC LogicalPlanGenerator::create_plan(SubQueryFilterStmt *filter_stmt, std::unique_ptr<LogicalOperator> &last_oper, std::unique_ptr<LogicalOperator> &logical_operator) {
  // SimpleFilter是用一个logical operator实现的，但是这里我们要用多个logical operator的父子组合实现sub query的and conjunction
  // 参数中last_oper就是下层算子返回的tuple
  RC rc = RC::SUCCESS;
  logical_operator = std::move(last_oper);
  for(const auto &unit: filter_stmt->filter_units()) {
    std::unique_ptr<SubQueryPredicateLogicalOperator> op = std::make_unique<SubQueryPredicateLogicalOperator>(unit->comp());
    if(unit->left().is_expr == false && unit->right().is_expr == false) { // 暂不支持两边都是subquery
      LOG_ERROR("both sides of query condition is sub query");
      return RC::INVALID_ARGUMENT;
    }

    std::unique_ptr<LogicalOperator> sub_query;
    if(unit->left().is_expr == false) {
      rc = create_plan(dynamic_cast<SelectStmt *>(unit->left().sub_query.get()), sub_query);
      if(OB_FAIL(rc)) {
        return rc;
      }
      // sub-query返回的tuple
      op->add_child(std::move(sub_query));
      // 下层算子返回的oper
      op->add_child(std::move(logical_operator));
      op->set_field(false, static_cast<FieldExpr *>(unit->right().expr.get())->field());
    } else {
      rc = create_plan(dynamic_cast<SelectStmt *>(unit->right().sub_query.get()), sub_query);
      if(OB_FAIL(rc)) {
        return rc;
      }
      // 下层算子返回的tuple
      op->add_child(std::move(logical_operator));
      // sub-query返回的tuple
      op->add_child(std::move(sub_query));
      op->set_field(true, static_cast<FieldExpr *>(unit->left().expr.get())->field());
    }

    // 更新返回的结果
    logical_operator = std::move(op);
  }
  return rc;
}


int LogicalPlanGenerator::implicit_cast_cost(AttrType from, AttrType to)
{
  if (from == to) {
    return 0;
  }
  return DataType::type_instance(from)->cast_cost(to);
}

RC LogicalPlanGenerator::create_plan(InsertStmt *insert_stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  Table        *table = insert_stmt->table();

  InsertLogicalOperator *insert_operator = new InsertLogicalOperator(table, insert_stmt->items());
  logical_operator.reset(insert_operator);
  return RC::SUCCESS;
}

RC LogicalPlanGenerator::create_plan(DeleteStmt *delete_stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  Table                      *table       = delete_stmt->table();
  SimpleFilterStmt                 *filter_stmt = delete_stmt->filter_stmt();
  unique_ptr<LogicalOperator> table_get_oper(new TableGetLogicalOperator(table, ReadWriteMode::READ_WRITE));

  unique_ptr<LogicalOperator> predicate_oper;

  RC rc = create_plan(filter_stmt, predicate_oper);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  unique_ptr<LogicalOperator> delete_oper(new DeleteLogicalOperator(table));

  if (predicate_oper) {
    predicate_oper->add_child(std::move(table_get_oper));
    delete_oper->add_child(std::move(predicate_oper));
  } else {
    delete_oper->add_child(std::move(table_get_oper));
  }

  logical_operator = std::move(delete_oper);
  return rc;
}

RC LogicalPlanGenerator::create_plan(ExplainStmt *explain_stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  unique_ptr<LogicalOperator> child_oper;

  Stmt *child_stmt = explain_stmt->child();

  RC rc = create(child_stmt, child_oper);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to create explain's child operator. rc=%s", strrc(rc));
    return rc;
  }

  logical_operator = unique_ptr<LogicalOperator>(new ExplainLogicalOperator);
  logical_operator->add_child(std::move(child_oper));
  return rc;
}

RC LogicalPlanGenerator::create_plan(UpdateStmt *update_stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  // filter
  unique_ptr<LogicalOperator> predicate_operator;
  RC rc = create_plan(update_stmt->filter_stmt(), predicate_operator);
  if(rc != RC::SUCCESS) {
    return rc;
  }

  // fetch
  unique_ptr<LogicalOperator> table_get_operator(new TableGetLogicalOperator(update_stmt->table(), ReadWriteMode::READ_WRITE));

  // compose to make an update logical operator
  unique_ptr<LogicalOperator> update_operator(new UpdateLogicalOperator(update_stmt->table(),
      update_stmt->attribute_name(), update_stmt->values(), update_stmt->value_amount()));
  if(predicate_operator) { // has a "where" clause
    predicate_operator->add_child(std::move(table_get_operator));
    update_operator->add_child(std::move(predicate_operator));
  } else { // no "where" clause
    update_operator->add_child(std::move(table_get_operator));
  }
  logical_operator = std::move(update_operator);

  return RC::SUCCESS;
}

// create plan for group by * or * aggregation
RC LogicalPlanGenerator::create_group_by_plan(SelectStmt *select_stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  vector<unique_ptr<Expression>> &group_by_expressions = select_stmt->group_by();
  vector<Expression *> aggregate_expressions;
  vector<unique_ptr<Expression>> &query_expressions = select_stmt->query_expressions();
  function<RC(std::unique_ptr<Expression>&)> collector = [&](unique_ptr<Expression> &expr) -> RC {
    RC rc = RC::SUCCESS;
    if (expr->type() == ExprType::AGGREGATION) {
      expr->set_pos(aggregate_expressions.size() + group_by_expressions.size());
      aggregate_expressions.push_back(expr.get()); // find all the aggregate expressions used in the query
    }
    rc = ExpressionIterator::iterate_child_expr(*expr, collector);
    return rc;
  };

  function<RC(std::unique_ptr<Expression>&)> bind_group_by_expr = [&](unique_ptr<Expression> &expr) -> RC {
    RC rc = RC::SUCCESS;
    for (size_t i = 0; i < group_by_expressions.size(); i++) {
      auto &group_by = group_by_expressions[i];
      if (expr->type() == ExprType::AGGREGATION) {
        break;
      } else if (expr->equal(*group_by)) {
        expr->set_pos(i); // find the group by expression in query expressions
        continue;
      } else {
        rc = ExpressionIterator::iterate_child_expr(*expr, bind_group_by_expr);
      }
    }
    return rc;
  };

  bool found_unbound_column = false;
  function<RC(std::unique_ptr<Expression>&)> find_unbound_column = [&](unique_ptr<Expression> &expr) -> RC {
    RC rc = RC::SUCCESS;
    if (expr->type() == ExprType::AGGREGATION) { // bound in aggregate function
      // do nothing
    } else if (expr->pos() != -1) { // already bound in group by
      // do nothing
    } else if (expr->type() == ExprType::FIELD) {
      found_unbound_column = true;
    }else {
      rc = ExpressionIterator::iterate_child_expr(*expr, find_unbound_column);
    }
    return rc;
  };
  

  for (unique_ptr<Expression> &expression : query_expressions) {
    bind_group_by_expr(expression);
  }

  for (unique_ptr<Expression> &expression : query_expressions) {
    find_unbound_column(expression); // find the query expressions that are not bound in group by or aggregate function
    // because they must appear in the GROUP BY clause or must be part of an aggregate function or they are invalid
    // 要么 query 的全在 group by 里，select a group by a 这样相当于对 a 去重；
    // 要么 query 的全在 aggregate 里, select agg(a) group by a 这样相当于对 a 聚合；
    // 要么 query 的全在 group by 里和 aggregate 里，select a, agg(b) group by a 这样相当于将所有 a 相同的一组之后再聚合；
    // 如果有不在 group by 里也不在 aggregate 里的，就是错误的，因为没法将这个值聚合到一个值上
  }

  // collect all aggregate expressions
  for (unique_ptr<Expression> &expression : query_expressions) {
    collector(expression);
  }

  if (group_by_expressions.empty() && aggregate_expressions.empty()) {
    // 既没有group by也没有聚合函数，不需要group by
    return RC::SUCCESS;
  }

  if (found_unbound_column) {
    LOG_WARN("column must appear in the GROUP BY clause or must be part of an aggregate function");
    return RC::INVALID_ARGUMENT;
  }

  // 如果只需要聚合，但是没有group by 语句，需要生成一个空的group by 语句

  auto group_by_oper = make_unique<GroupByLogicalOperator>(std::move(group_by_expressions),
                                                           std::move(aggregate_expressions));
  logical_operator = std::move(group_by_oper);
  return RC::SUCCESS;
}