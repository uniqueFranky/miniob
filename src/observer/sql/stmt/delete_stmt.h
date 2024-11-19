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
// Created by Wangyunlai on 2022/5/27.
//

#pragma once

#include "sql/parser/parse_defs.h"
#include "sql/stmt/stmt.h"
#include "filter_stmt.h"

class Table;
template<typename FilterObj>
class FilterStmt;

/**
 * @brief Delete 语句
 * @ingroup Statement
 * 目前不支持delete的sub query
 */
class DeleteStmt : public Stmt
{
public:
  DeleteStmt(Table *table, SimpleFilterStmt *filter_stmt);
  ~DeleteStmt() override;

  Table      *table() const { return table_; }
  SimpleFilterStmt *filter_stmt() const { return filter_stmt_; }

  StmtType type() const override { return StmtType::DELETE; }

public:
  static RC create(Db *db, const DeleteSqlNode &delete_sql, Stmt *&stmt);

private:
  Table      *table_       = nullptr;
  SimpleFilterStmt *filter_stmt_ = nullptr;
};
