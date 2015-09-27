#pragma once

#include <string>
#include <memory>
#include <vector>

#include <wangle/concurrent/CPUThreadPoolExecutor.h>
#include <wangle/concurrent/FutureExecutor.h>
#include <folly/futures/Future.h>
#include "CollectionDBHandle.h"

#include "util.h"

namespace persistence {

class CollectionDBIf {
public:

  virtual void initialize() = 0;

  virtual folly::Future<bool>
    doesCollectionExist(const std::string&) = 0;
  virtual folly::Future<bool>
    createCollection(const std::string&) = 0;

  virtual folly::Future<bool>
    doesCollectionHaveDocument(const std::string&, const std::string&) = 0;

  virtual folly::Future<bool>
    addPositiveDocumentToCollection(const std::string&, const std::string&) = 0;

  virtual folly::Future<bool>
    addNegativeDocumentToCollection(const std::string&, const std::string&) = 0;

  virtual folly::Future<bool>
    removeDocumentFromCollection(const std::string&, const std::string&) = 0;

  virtual folly::Future<bool>
    deleteCollection(const std::string&) = 0;

  virtual folly::Future<std::vector<std::string>>
    listCollections() = 0;

  virtual folly::Future<int>
    getCollectionDocumentCount(const std::string &) = 0;

  virtual folly::Future<std::vector<std::string>>
    listCollectionDocuments(const string &collectionId) = 0;

  virtual folly::Future<std::vector<std::string>>
    listPositiveCollectionDocuments(const string &collectionId) = 0;

  virtual folly::Future<std::vector<std::string>>
    listNegativeCollectionDocuments(const string &collectionId) = 0;

  virtual folly::Future<std::vector<std::string>>
    listKnownDocuments() = 0;

  virtual ~CollectionDBIf() = default;
};

class CollectionDB: public CollectionDBIf {
protected:
  util::UniquePointer<CollectionDBHandleIf> dbHandle_;
  std::shared_ptr<wangle::FutureExecutor<wangle::CPUThreadPoolExecutor>> threadPool_;
  CollectionDB(CollectionDB const&) = delete;
  void operator=(CollectionDB const&) = delete;
public:
  CollectionDB(
    util::UniquePointer<CollectionDBHandleIf> dbHandle,
    std::shared_ptr<wangle::FutureExecutor<wangle::CPUThreadPoolExecutor>> threadPool
  );
  void initialize() override;
  folly::Future<bool> doesCollectionExist(const std::string &collectionId);
  folly::Future<bool> createCollection(const std::string &collectionId);
  folly::Future<bool> doesCollectionHaveDocument(const std::string &collectionId, const std::string &docId);
  folly::Future<bool> addPositiveDocumentToCollection(const std::string &collectionId, const std::string &docId);
  folly::Future<bool> addNegativeDocumentToCollection(const std::string &collectionId, const std::string &docId);
  folly::Future<bool> removeDocumentFromCollection(const std::string &collectionId, const std::string &docId);
  folly::Future<bool> deleteCollection(const std::string &collectionId);
  folly::Future<std::vector<std::string>> listCollections();
  folly::Future<int> getCollectionDocumentCount(const std::string &collectionId);
  folly::Future<std::vector<std::string>> listCollectionDocuments(const std::string &collectionId);
  folly::Future<std::vector<std::string>> listPositiveCollectionDocuments(const std::string &collectionId);
  folly::Future<std::vector<std::string>> listNegativeCollectionDocuments(const std::string &collectionId);
  folly::Future<std::vector<std::string>> listKnownDocuments();
};

} // persistence
