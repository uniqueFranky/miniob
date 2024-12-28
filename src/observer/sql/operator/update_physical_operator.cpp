//
// Created by 闫润邦 on 2024/9/16.
//

#include "common/lang/algorithm.h"
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
    if (field_meta->type() == AttrType::TEXTS && values_[0].attr_type() == AttrType::CHARS) {
      // do nothing
    } 
    else if(DataType::type_instance(field_meta->type())->cast_cost(values_[0].attr_type())!= INT32_MAX) {
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

  // fix: 在写之前释放读锁
  rc = children_.front()->close();
  if(OB_FAIL(rc)) {
    return rc;
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
    if (field_meta->type() == AttrType::TEXTS) {
      PageNum pre_page_num = *reinterpret_cast<PageNum *>(record.data() + field_meta->offset());
      DiskBufferPool *dbp = table_->data_buffer_pool();
      if (values_[0].is_null()) { // 不过貌似其它类型都不支持 update 成 null
        // 如果新值是null，那么直接将page_num置为BP_INVALID_PAGE_NUM
        PageNum invalid_page_num = BP_INVALID_PAGE_NUM;
        memcpy(record.data() + field_meta->offset(), &invalid_page_num, sizeof(PageNum));
        field_meta->set_field_null_indicator(record.data(), true);
        // 释放原来的page
        if (pre_page_num != BP_INVALID_PAGE_NUM) {
          Frame *frame = nullptr;
          rc = dbp->get_this_page(pre_page_num, &frame);
          if (OB_FAIL(rc)) {
            LOG_WARN("Failed to get page %d from buffer pool when updating. rc=%s", pre_page_num, strrc(rc));
            return rc;
          }
          frame->pin();
          frame->mark_dirty();
          frame->unpin();
          dbp->dispose_page(pre_page_num);
        }
      } 
      else if (pre_page_num != BP_INVALID_PAGE_NUM) {
        // 不用释放原来的page，因为这个page会被新的text字段引用
        Frame *frame = nullptr;
        rc = dbp->get_this_page(pre_page_num, &frame);
        if (OB_FAIL(rc)) {
          LOG_WARN("Failed to get page %d from buffer pool when updating. rc=%s", pre_page_num, strrc(rc));
          return rc;
        }
        frame->pin();
        memcpy(frame->data(), values_[0].data(), min(4096, values_[0].length()));
        field_meta->set_field_null_indicator(record.data(), false);
        frame->unpin();
      }
      else {
        // 需要分配新的page
        Frame *frame = nullptr;
        rc = dbp->allocate_page(&frame);
        if (OB_FAIL(rc)) {
          LOG_WARN("Failed to allocate page when updating. rc=%s", strrc(rc));
          return rc;
        }
        frame->pin();
        memcpy(frame->data(), values_[0].data(), min(4096, values_[0].length()));
        frame->mark_dirty();
        frame->unpin();
        // 更新record中的page_num
        PageNum new_page_num = frame->page_num();
        memcpy(record.data() + field_meta->offset(), &new_page_num, sizeof(PageNum));
        field_meta->set_field_null_indicator(record.data(), false);
      }
    } 
    else {
      rc = record.set_field(field_meta->offset(), field_meta->len() - 1, const_cast<char *>(values_[0].data()));
    }
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