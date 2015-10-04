#pragma once

#include <string>
#include "models/WordVector.h"
namespace relevanced {
namespace models {

class ProcessedDocument {
public:
  std::string id;
  WordVector wordVector;
  ProcessedDocument(){}
  ProcessedDocument(std::string id): id(id){}
  ProcessedDocument(std::string id, WordVector wordVec): id(id), wordVector(wordVec){}
  ProcessedDocument(std::string id, std::map<std::string, double> scores, double mag):
    id(id),  wordVector(scores, mag) {}
};

} // models
} // relevanced

namespace folly {

  using relevanced::models::ProcessedDocument;
  using relevanced::models::WordVector;

  template<>
  struct DynamicConstructor<ProcessedDocument> {
    static folly::dynamic construct(const ProcessedDocument &doc) {
      auto wordVec = folly::toDynamic(doc.wordVector);
      folly::dynamic self = folly::dynamic::object;
      self["id"] = doc.id;
      return self;
    }
  };

  template<>
  struct DynamicConverter<ProcessedDocument> {
    static ProcessedDocument convert(const folly::dynamic &dyn) {
      auto wordVec = folly::convertTo<WordVector>(dyn["wordVector"]);
      auto id = folly::convertTo<std::string>(dyn["id"]);
      return ProcessedDocument(id, wordVec);
    }
  };
} // folly


namespace relevanced {
namespace serialization {

  using models::WordVector;
  using models::ProcessedDocument;


  template<>
  struct BinarySerializer<ProcessedDocument> {
    static void serialize(std::string &result, ProcessedDocument &target) {\
      services::ProcessedDocumentDTO docDto;
      docDto.wordVector.scores = target.wordVector.scores;
      docDto.wordVector.magnitude = target.wordVector.magnitude;
      docDto.wordVector.documentWeight = target.wordVector.documentWeight;
      docDto.id = target.id;
      serialization::thriftBinarySerialize(result, docDto);
    }
  };

  template<>
  struct BinaryDeserializer<ProcessedDocument> {
    static void deserialize(std::string &data, ProcessedDocument *result) {
      services::ProcessedDocumentDTO docDto;
      serialization::thriftBinaryDeserialize(data, docDto);
      result->id = docDto.id;
      result->wordVector.scores = docDto.wordVector.scores;
      result->wordVector.magnitude = docDto.wordVector.magnitude;
      result->wordVector.documentWeight = docDto.wordVector.documentWeight;
    }
  };


  template<>
  struct JsonSerializer<ProcessedDocument> {
    static std::string serialize(ProcessedDocument *document) {
      auto dynSelf = folly::toDynamic<ProcessedDocument>(*document);
      folly::fbstring js = folly::toJson(dynSelf);
      return js.toStdString();
    }
  };

  template<>
  struct JsonDeserializer<ProcessedDocument> {
    static ProcessedDocument deserialize(const std::string &js) {
      auto dynSelf = folly::parseJson(js);
      return folly::convertTo<ProcessedDocument>(dynSelf);
    }
  };

} // serialization
} // relevanced