/* Copyright (c) 2021OceanBase and/or its affiliates. All rights reserved.
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

#include "sql/stmt/insert_stmt.h"

#include <utility>
#include "common/log/log.h"
#include "storage/db/db.h"
#include "storage/table/table.h"

InsertStmt::InsertStmt(Table *table, vector<vector<Value>> items)
    : table_(table), items_(std::move(items))
{}

RC InsertStmt::create(Db *db, InsertSqlNode &inserts, Stmt *&stmt)
{
  const char *table_name = inserts.relation_name.c_str();
  if (nullptr == db || nullptr == table_name || inserts.items.empty()) {
    LOG_WARN("invalid argument. db=%p, table_name=%p, item_num=%d",
        db, table_name, static_cast<int>(inserts.items.size()));
    return RC::INVALID_ARGUMENT;
  }

  // check whether the table exists
  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  // check the fields number
  const TableMeta &table_meta = table->table_meta();
  const int        field_num  = table_meta.field_num() - table_meta.sys_field_num();
  for(const vector<Value> &item: inserts.items) {
    int value_num = static_cast<int>(item.size());
    if(field_num != value_num) {
      LOG_WARN("schema mismatch. value num=%d, field num in schema=%d", value_num, field_num);
      return RC::SCHEMA_FIELD_MISSING;
    }

    // check the type of each field
    for(int i = 0; i < field_num; i++) {
      const FieldMeta *field_meta = table_meta.field(i + table_meta.sys_field_num());
      const AttrType field_type = field_meta->type();
      const AttrType value_type = item[i].attr_type();
      if (value_type == AttrType::NULLS && field_meta->nullable()) {
        continue;
      }
      if(value_type != field_type) {
        if (field_type == AttrType::TEXTS && value_type == AttrType::CHARS) {
          if (MAX_TEXT_LENGTH < item[i].length()) {
            LOG_WARN("Text length is too long. field_name=%s, length=%d, max_length=%d", field_meta->name(), item[i].length(), MAX_TEXT_LENGTH);
            return RC::INVALID_ARGUMENT;
          }
        } else {
          LOG_WARN("schema mismatch. field type mismatch. field_name=%s, expected=%s, actual=%s", field_meta->name(), attr_type_to_string(field_meta->type()), attr_type_to_string(item[i].attr_type()));
          return RC::SCHEMA_FIELD_TYPE_MISMATCH;
        }
      }
      if (field_type == AttrType::CHARS) {
        if (item[i].length() > field_meta->len()) {
          LOG_WARN("Text length is too long. field_name=%s, length=%d, max_length=%d",
            field_meta->name(), item[i].length(), field_meta->len());
          return RC::INVALID_ARGUMENT;
        }
        char *char_value = (char*)malloc(field_meta->len());
        memset(char_value, 0, field_meta->len());
        memcpy(char_value, item[i].data(), item[i].length());
        const_cast<Value&>(item[i]).set_data(char_value, field_meta->len());
        free(char_value);
      }
    }
  }
  // everything alright
  stmt = new InsertStmt(table, std::move(inserts.items));
  return RC::SUCCESS;
}
