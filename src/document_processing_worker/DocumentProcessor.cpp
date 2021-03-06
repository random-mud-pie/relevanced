#include <memory>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <tuple>
#include <cmath>
#include <glog/logging.h>
#include "DocumentProcessor.h"
#include "models/Document.h"
#include "models/ProcessedDocument.h"
#include "models/WordVector.h"
#include "stopwords/StopwordFilter.h"
#include "stemmer/StemmerManagerIf.h"
#include "tokenizer/DestructiveTokenIterator.h"
#include "util/Clock.h"
#include "util/util.h"
#include "text_util/ScoredWord.h"
#include "text_util/StringView.h"
#include "text_util/WordAccumulator.h"


namespace relevanced {
namespace document_processing_worker {

using models::Document;
using models::ProcessedDocument;
using stemmer::StemmerManagerIf;
using namespace std;
using util::UniquePointer;
using util::ClockIf;
using stopwords::StopwordFilterIf;
using namespace relevanced::text_util;

DocumentProcessor::DocumentProcessor(
    shared_ptr<StemmerManagerIf> stemmerManager,
    shared_ptr<StopwordFilterIf> stopwordFilter,
    shared_ptr<ClockIf> clock
  ): stemmerManager_(stemmerManager),
     stopwordFilter_(stopwordFilter),
     clock_(clock) {}

void DocumentProcessor::process_(
    Document &doc, ProcessedDocument *result) {
  tokenizer::DestructiveTokenIterator it(doc.text);
  WordAccumulator accumulator {200};
  const char *cStr = doc.text.c_str();
  std::tuple<bool, size_t, size_t> tokenOffsets;
  auto stemmer = stemmerManager_->getStemmer(doc.language);
  while (it.next(tokenOffsets)) {
    if (!std::get<0>(tokenOffsets)) {
      break;
    }
    size_t startPos = ::std::get<1>(tokenOffsets);
    size_t len = ::std::get<2>(tokenOffsets) - startPos;
    const char *tokenStart = cStr + startPos;
    len = stemmer->getStemPos(tokenStart, len);
    if (len > 23) {
      len = 23;
    }
    StringView view(tokenStart, len);
    uint8_t wordLen = (uint8_t) len;
    ScoredWord scored(tokenStart, wordLen);
    if (stopwordFilter_->isStopword(scored.word, doc.language)) {
      continue;
    }
    accumulator.add(view);
  }
  accumulator.build();
  result->magnitude = accumulator.getMagnitude();
  result->scoredWords = std::move(accumulator.getScores());
  result->id = doc.id;
  auto timestamp = clock_->getEpochTime();
  result->created = timestamp;
  result->updated = timestamp;
}

void DocumentProcessor::process_(
    Document &doc, shared_ptr<ProcessedDocument> result) {
  return process_(doc, result.get());
}

ProcessedDocument DocumentProcessor::process(Document &doc) {
  ProcessedDocument processed(doc.id);
  process_(doc, &processed);
  return processed;
}

shared_ptr<ProcessedDocument> DocumentProcessor::processNew(
    Document &doc) {
  auto result = std::make_shared<ProcessedDocument>(doc.id);
  process_(doc, result);
  return result;
}

shared_ptr<ProcessedDocument> DocumentProcessor::processNew(
    shared_ptr<Document> doc) {
  auto result = std::make_shared<ProcessedDocument>(doc->id);
  process_(*doc, result);
  return result;
}

} // document_processing_worker
} // relevanced