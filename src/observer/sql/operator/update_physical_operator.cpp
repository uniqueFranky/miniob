//
// Created by 闫润邦 on 2024/9/16.
//

#include "sql/operator/update_physical_operator.h"
#include "storage/trx/trx.h"

RC UpdatePhysicalOperator::open(Trx *trx)
{
  ASSERT(children_.size() == 1, "UpdatePhysicalOperator must exactly have 1 child.");
  ASSERT(value_amount_ == 1, "UpdatePhysicalOperator only supports 1 field.");

  const FieldMeta *field_meta = table_->table_meta().field(attribute_name_.c_str());
  if(field_meta == nullptr) { // Field必须存在
    LOG_WARN("Field not exists.");
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }
  if(values_[0].attr_type() != field_meta->type()) { // 类型必须匹配
    // 尝试进行类型转换
    if(DataType::type_instance(field_meta->type())->cast_cost(values_[0].attr_type())!= INT32_MAX) {
      RC rc = Value::cast_to(values_[0], field_meta->type(), values_[0]);
      if(OB_FAIL(rc)) {
        LOG_WARN("Failed to convert type %d to %d.", values_[0].attr_type(), field_meta->type());
        return rc;
      }
    } else {
      LOG_WARN("Field type not match.");
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }

  }

  // 打开子算子，即 predicate 或table get
  RC rc = children_.front()->open(trx);
  if(OB_FAIL(rc)) {
    LOG_WARN("Failed to open child physical operator.");
    return rc;
  }

  vector<Record> records; // 待修改数组
  // 调用子算子来遍历符合条件的元组，加入到待修改数组中
  while(OB_SUCC(rc = children_.front()->next())) {
    Tuple *tuple = children_.front()->current_tuple();
    if(tuple == nullptr) {
      LOG_WARN("Failed to get current record.");
      return RC::INTERNAL;
    }
    records.emplace_back(static_cast<RowTuple *>(tuple)->record());
  }

  for(Record &record: records) {
    // 删除原记录
    rc = trx->delete_record(table_, record);
    if(OB_FAIL(rc)) {
      LOG_WARN("Failed to delete original record.");
      return rc;
    }
    // 修改记录
    record.copy_data(record.data(), record.len()); // cannot set field when record does not own the memory
    rc = record.set_field(field_meta->offset(), field_meta->len() - 1, const_cast<char *>(values_[0].data()));
    if(OB_FAIL(rc)) {
      LOG_WARN("Failed to update record.");
      return rc;
    }
    // 插入新记录
    rc = trx->insert_record(table_, record);
    if(OB_FAIL(rc)) {
      LOG_WARN("Failed to insert new record");
      return rc;
    }
  }
  return RC::SUCCESS;
}

RC UpdatePhysicalOperator::next()
{
  return RC::RECORD_EOF;
}

RC UpdatePhysicalOperator::close()
{
  return RC::SUCCESS;
}