#pragma once

#include "common/rc.h"
#include "common/types.h"
#include "common/type/data_type.h"

/**
 * @brief 不固定长度的字符串类型
 * @ingroup DataType
 */
class TextType : public DataType
{
public:
    TextType() : DataType(AttrType::TEXTS) {}

    virtual ~TextType() = default;

    int compare(const Value &left, const Value &right) const override;

    RC cast_to(const Value &val, AttrType type, Value &result) const override;

    RC set_value_from_str(Value &val, const string &data) const override;

    int cast_cost(AttrType type) override;

    RC to_string(const Value &val, string &result) const override;
private:
    static const int MAX_TEXT_LENGTH = 4096;
};