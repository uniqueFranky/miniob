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

#pragma once

#include "sql/expr/expression.h"
#include "sql/parser/parse_defs.h"
#include "sql/stmt/stmt.h"
#include "sql/stmt/select_stmt.h"
#include <unordered_map>
#include <vector>

class Db;
class Table;
class FieldMeta;
class SelectStmt;

RC get_table_and_field(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
    const RelAttrSqlNode &attr, Table *&table, const FieldMeta *&field);

struct SimpleFilterObj
{
  bool  is_attr;
  Field field;
  Value value;

  void init_attr(const Field &field)
  {
    is_attr     = true;
    this->field = field;
  }

  void init_value(const Value &value)
  {
    is_attr     = false;
    this->value = value;
  }
};

struct SubQueryFilterObj {
  bool is_attr;
  Field field;
  // 一定是QueryStmt
  std::unique_ptr<Stmt> sub_query;

  void init_attr(const Field &field) {
    is_attr = true;
    this->field = field;
  }

  void init_sub_query(std::unique_ptr<Stmt> sub_query) {
    is_attr = false;
    this->sub_query = std::move(sub_query);
  }
};

template<typename FilterObj>
class FilterUnit
{
public:
  FilterUnit() = default;
  ~FilterUnit() {}

  void set_comp(CompOp comp) { comp_ = comp; }

  CompOp comp() const { return comp_; }

  void set_left(FilterObj obj) { left_ = std::move(obj); }
  void set_right(FilterObj obj) { right_ = std::move(obj); }

  const FilterObj &left() const { return left_; }
  const FilterObj &right() const { return right_; }

private:
  CompOp    comp_ = NO_OP;
  FilterObj left_;
  FilterObj right_;
};

/**
 * @brief Filter/谓词/过滤语句
 * @ingroup Statement
 */
template<typename FilterObj>
class FilterStmt
{
public:
  FilterStmt() = default;
  virtual ~FilterStmt() {
    for (FilterUnit<FilterObj> *unit : filter_units_) {
      delete unit;
    }
    filter_units_.clear();
  }

public:
  const std::vector<FilterUnit<FilterObj> *> &filter_units() const { return filter_units_; }
  static RC create(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
                                        const ConditionSqlNode *conditions, int condition_num, FilterStmt *&stmt) = delete;
  static RC create_filter_unit(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
                                        const ConditionSqlNode &condition, FilterUnit<SimpleFilterObj> *&filter_unit) = delete;
protected:
  std::vector<FilterUnit<FilterObj> *> filter_units_;  // 默认当前都是AND关系
};

class SimpleFilterStmt: public FilterStmt<SimpleFilterObj> {
public:
  SimpleFilterStmt() = default;
public:
  static RC create(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
      const ConditionSqlNode *conditions, int condition_num, SimpleFilterStmt *&stmt);
  static RC create_filter_unit(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
      const ConditionSqlNode &condition, FilterUnit<SimpleFilterObj> *&filter_unit);
};

class SubQueryFilterStmt: public FilterStmt<SubQueryFilterObj> {
public:
  SubQueryFilterStmt() = default;
public:
  static RC create(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
      const ConditionSqlNode *conditions, int condition_num, SubQueryFilterStmt *&stmt);
  static RC create_filter_unit(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
      const ConditionSqlNode &condition, FilterUnit<SubQueryFilterObj> *&filter_unit);
};
