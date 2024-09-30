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
// Created by WangYunlai on 2021/6/9.
//

#include "sql/operator/insert_physical_operator.h"
#include "sql/stmt/insert_stmt.h"
#include "storage/table/table.h"
#include "storage/trx/trx.h"
#include "storage/index/index.h"

using namespace std;

InsertPhysicalOperator::InsertPhysicalOperator(Table *table, vector<vector<Value>> &&items)
    : table_(table), items_(std::move(items))
{}

RC InsertPhysicalOperator::open(Trx *trx)
{
  RC rc = RC::SUCCESS;
  vector<Record> records;
  for(const vector<Value> &item: items_) {
    Record record;
    rc = table_->make_record(static_cast<int>(item.size()), item.data(), record);
    if(OB_FAIL(rc)) {
      LOG_WARN("failed to make record. rc=%s", strrc(rc));
      return rc;
    }

    // check for uniqueness
    // fixme: currently omitting the duplicate keys in the same insert statement
    for(int i = 0; i < table_->table_meta().unique_index_num(); i++) {
      const IndexMeta *unique_index_meta = table_->table_meta().unique_index(i);
      Index *unique_index = table_->find_index(unique_index_meta->name());
      /* omit null values */
      const FieldMeta *field = table_->table_meta().field(unique_index_meta->field());
      if(field->is_field_null(record.data())) {
        continue;
      }
      /* check uniqueness for non-null values */
      list<RID> rids;
      rc = unique_index->get_entry(record.data(), rids);
      if(OB_FAIL(rc)) {
        LOG_WARN("failed to look up for record when inserting into table with unique index. rc=%s", strrc(rc));
        return rc;
      }
      if(!rids.empty()) {
        LOG_WARN("inserting duplicate keys into a table with unique index.");
        return RC::INVALID_ARGUMENT;
      }
    }
    // make sure every item is valid
    records.emplace_back(std::move(record));
  }

  for(Record &record: records) {
    rc = trx->insert_record(table_, record);
    if(OB_FAIL(rc)) {
      LOG_WARN("failed to insert record by transaction. rc=%s", strrc(rc));
      return rc;
    }
  }
  return RC::SUCCESS;
}

RC InsertPhysicalOperator::next() { return RC::RECORD_EOF; }

RC InsertPhysicalOperator::close() { return RC::SUCCESS; }
