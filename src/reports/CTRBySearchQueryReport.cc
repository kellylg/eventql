/**
 * Copyright (c) 2015 - The CM Authors <legal@clickmatcher.com>
 *   All Rights Reserved.
 *
 * This file is CONFIDENTIAL -- Distribution or duplication of this material or
 * the information contained herein is strictly forbidden unless prior written
 * permission is obtained.
 */
#include "reports/CTRBySearchQueryReport.h"

using namespace fnord;

namespace cm {

CTRBySearchQueryReport::CTRBySearchQueryReport(
    RefPtr<JoinedQueryTableSource> input,
    RefPtr<CTRCounterTableSink> output,
    ItemEligibility eligibility,
    RefPtr<fts::Analyzer> analyzer) :
    Report(input.get(), output.get()),
    joined_queries_(input),
    ctr_table_(output),
    eligibility_(eligibility),
    analyzer_(analyzer) {}

void CTRBySearchQueryReport::onInit() {
  joined_queries_->forEach(std::bind(
      &CTRBySearchQueryReport::onJoinedQuery,
      this,
      std::placeholders::_1));
}

void CTRBySearchQueryReport::onJoinedQuery(const JoinedQuery& q) {
  if (!isQueryEligible(eligibility_, q)) {
    return;
  }

  auto lang = extractLanguage(q.attrs);
  auto lang_str = languageToString(lang);
  uint64_t num_clicks = 0;
  for (auto& item : q.items) {
    if (!isItemEligible(eligibility_, q, item)) {
      continue;
    }

    if (item.clicked) ++num_clicks;
  }


  auto qstr = extractAttr(q.attrs, "qstr~" + lang_str);
  if (qstr.isEmpty()) {
    return;
  }

  auto qstr_norm = analyzer_->normalize(lang, qstr.get());

  Set<String> keys;
  keys.emplace("all");
  keys.emplace(lang_str);
  keys.emplace(StringUtil::format(
      "$0~$1",
      lang_str,
      qstr_norm));

  for (const auto& key : keys) {
    auto& ctr = counters_[key];
    if (num_clicks > 0) ++ctr.num_clicks;
    ctr.num_clicked += num_clicks;
  }
}

void CTRBySearchQueryReport::onFinish() {
  for (auto& ctr : counters_) {
    ctr_table_->addRow(ctr.first, ctr.second);
  }
}


} // namespace cm

