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

#include "sql/stmt/filter_stmt.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "common/rc.h"
#include "storage/db/db.h"
#include "storage/table/table.h"

RC get_table_and_field(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
    const RelAttrSqlNode &attr, Table *&table, const FieldMeta *&field)
{
  if (common::is_blank(attr.relation_name.c_str())) {
    table = default_table;
  } else if (nullptr != tables) {
    auto iter = tables->find(attr.relation_name);
    if (iter != tables->end()) {
      table = iter->second;
    }
  } else {
    table = db->find_table(attr.relation_name.c_str());
  }
  if (nullptr == table) {
    LOG_WARN("No such table: attr.relation_name: %s", attr.relation_name.c_str());
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  field = table->table_meta().field(attr.attribute_name.c_str());
  if (nullptr == field) {
    LOG_WARN("no such field in table: table %s, field %s", table->name(), attr.attribute_name.c_str());
    table = nullptr;
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }

  return RC::SUCCESS;
}

RC SimpleFilterStmt::create(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
    ConditionSqlNode *conditions, int condition_num, SimpleFilterStmt *&stmt) {
  RC rc = RC::SUCCESS;
  stmt  = nullptr;

  SimpleFilterStmt *tmp_stmt = new SimpleFilterStmt();
  for (int i = 0; i < condition_num; i++) {
    FilterUnit<SimpleFilterObj> *filter_unit = nullptr;

    rc = create_filter_unit(db, default_table, tables, conditions[i], filter_unit);
    if (rc != RC::SUCCESS) {
      delete tmp_stmt;
      LOG_WARN("failed to create filter unit. condition index=%d", i);
      return rc;
    }
    tmp_stmt->filter_units_.push_back(filter_unit);
  }

  stmt = tmp_stmt;
  return rc;
}

RC SimpleFilterStmt::create_filter_unit(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
    ConditionSqlNode &condition, FilterUnit<SimpleFilterObj> *&filter_unit) {
  RC rc = RC::SUCCESS;

  CompOp comp = condition.comp;
  if (comp < EQUAL_TO || comp >= NO_OP) {
    LOG_WARN("invalid compare operator : %d", comp);
    return RC::INVALID_ARGUMENT;
  }

  filter_unit = new FilterUnit<SimpleFilterObj>;
  filter_unit->set_left(SimpleFilterObj(std::move(condition.left_expression)));
  filter_unit->set_comp(comp);
  filter_unit->set_right(SimpleFilterObj(std::move(condition.right_expression)));

  // 检查两个类型是否能够比较
  return rc;
}

RC SubQueryFilterStmt::create(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
    ConditionSqlNode *conditions, int condition_num, SubQueryFilterStmt *&stmt) {
  RC rc = RC::SUCCESS;
  stmt  = nullptr;

  SubQueryFilterStmt *tmp_stmt = new SubQueryFilterStmt();
  for (int i = 0; i < condition_num; i++) {
    FilterUnit<SubQueryFilterObj> *filter_unit = nullptr;

    rc = create_filter_unit(db, default_table, tables, conditions[i], filter_unit);
    if (rc != RC::SUCCESS) {
      delete tmp_stmt;
      LOG_WARN("sub query failed to create filter unit. condition index=%d", i);
      return rc;
    }
    tmp_stmt->filter_units_.push_back(filter_unit);
  }

  stmt = tmp_stmt;
  return rc;
}

RC SubQueryFilterStmt::create_filter_unit(Db *db, Table *default_table, std::unordered_map<std::string, Table *> *tables,
    ConditionSqlNode &condition, FilterUnit<SubQueryFilterObj> *&filter_unit) {
  RC rc = RC::SUCCESS;

  CompOp comp = condition.comp;
  if (comp < EQUAL_TO || comp >= NO_OP) {
    LOG_WARN("invalid compare operator : %d", comp);
    return RC::INVALID_ARGUMENT;
  }

  filter_unit = new FilterUnit<SubQueryFilterObj>;

  switch(condition.left_type) {
    case ConditionSqlNode::SideType::Expr: {
      filter_unit->set_left(SubQueryFilterObj(std::move(condition.left_expression)));
    } break;
    case ConditionSqlNode::SideType::SubQuery: {
      Stmt *sub_query;
      rc = SelectStmt::create(db, *condition.left_sub_query, sub_query);
      if(OB_FAIL(rc)) {
        return rc;
      }
      filter_unit->set_left(SubQueryFilterObj(std::unique_ptr<Stmt>(sub_query)));
    } break;
    default: {
      return RC::INVALID_ARGUMENT;
    }
  }

  switch(condition.right_type) {
    case ConditionSqlNode::SideType::Expr: {
      filter_unit->set_left(SubQueryFilterObj(std::move(condition.right_expression)));
    } break;
    case ConditionSqlNode::SideType::SubQuery: {
      Stmt *sub_query;
      rc = SelectStmt::create(db, *condition.right_sub_query, sub_query);
      if(OB_FAIL(rc)) {
        return rc;
      }
      filter_unit->set_left(SubQueryFilterObj(std::unique_ptr<Stmt>(sub_query)));
    } break;
    default: {
      return RC::INVALID_ARGUMENT;
    }
  }

  filter_unit->set_comp(comp);

  // 检查两个类型是否能够比较
  return rc;
}