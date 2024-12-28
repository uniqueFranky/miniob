#include "common/lang/comparator.h"
#include "common/log/log.h"
#include "common/type/text_type.h"
#include "common/value.h"

int TextType::compare(const Value &left, const Value &right) const
{
    ASSERT(left.attr_type() == AttrType::TEXTS && right.attr_type() == AttrType::TEXTS, "invalid type");
    std::string left_str, right_str;
    to_string(left, left_str);
    to_string(right, right_str);
    return common::compare_string((void*)left_str.c_str(), left_str.length(), (void*)right_str.c_str(), right_str.length());
}

RC TextType::set_value_from_str(Value &val, const std::string &data) const
{
    // only called by read from file
    val.set_text(std::stoi(data));
    return RC::SUCCESS;
}

RC TextType::cast_to(const Value &val, AttrType type, Value &result) const
{
    switch (type) {
        case AttrType::CHARS: {
            result.set_string(val.get_text().c_str());
            return RC::SUCCESS;
        }
        case AttrType::DATES: {
            if (Date_t::is_valid_string_for_date(val.get_text().c_str())) {
                result.set_date(Date_t::parse_date_from_string(val.get_text().c_str()));
                return RC::SUCCESS;
            } else {
                return RC::INVALID_ARGUMENT;
            }
        }
        default: return RC::UNIMPLEMENTED;
    }
    return RC::SUCCESS;
}

int TextType::cast_cost(AttrType type)
{
    if (type == AttrType::TEXTS) {
        return 0;
    } else if (type == AttrType::CHARS) {
        return 1;
    } else if (type == AttrType::DATES) {
        return 2;
    }
    return INT32_MAX;
}

RC TextType::to_string(const Value &val, std::string &result) const
{
    result = val.get_text();
    return RC::SUCCESS;
}