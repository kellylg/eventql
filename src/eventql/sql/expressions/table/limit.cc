/**
 * This file is part of the "libfnord" project
 *   Copyright (c) 2015 Paul Asmuth
 *
 * FnordMetric is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <eventql/sql/expressions/table/limit.h>

namespace csql {

LimitExpression::LimitExpression(
    size_t limit,
    size_t offset,
    ScopedPtr<TableExpression> input) :
    limit_(limit),
    offset_(offset),
    input_(std::move(input)),
    counter_(0) {}

ScopedPtr<ResultCursor> LimitExpression::execute() {
  input_cursor_ = input_->execute();
  buf_.resize(input_cursor_->getNumColumns());

  return mkScoped(
      new DefaultResultCursor(
          input_cursor_->getNumColumns(),
          std::bind(
              &LimitExpression::next,
              this,
              std::placeholders::_1,
              std::placeholders::_2)));
}

bool LimitExpression::next(SValue* row, size_t row_len) {
  if (limit_ == 0 || counter_ >= offset_ + limit_) {
    return false;
  }

  while (input_cursor_->next(buf_.data(), buf_.size())) {
    if (counter_++ < offset_) {
      continue;
    } else {
      for (size_t i = 0; i < row_len && i < buf_.size(); ++i) {
        row[i] = buf_[i];
      }
      return true;
    }
  }
  
  return false;
}

}