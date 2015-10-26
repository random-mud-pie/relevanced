#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <vector>
#include <string>
#include <chrono>
#include <unordered_map>
#include <memory>

#include <glog/logging.h>

#include <folly/ExceptionWrapper.h>
#include <folly/Memory.h>
#include <folly/futures/Try.h>
#include <folly/futures/Future.h>
#include <folly/futures/helpers.h>
#include <folly/Optional.h>

#include <wangle/concurrent/CPUThreadPoolExecutor.h>
#include <wangle/concurrent/FutureExecutor.h>

#include "testing/TestHelpers.h"
#include "gen-cpp2/RelevancedProtocol_types.h"
#include "persistence/Persistence.h"
#include "persistence/SyncPersistence.h"
#include "persistence/CentroidMetadataDb.h"
#include "persistence/InMemoryRockHandle.h"
#include "centroid_update_worker/CentroidUpdateWorker.h"
#include "centroid_update_worker/CentroidUpdaterFactory.h"
#include "centroid_update_worker/DocumentAccumulatorFactory.h"
#include "document_processing_worker/DocumentProcessor.h"
#include "document_processing_worker/DocumentProcessingWorker.h"
#include "similarity_score_worker/SimilarityScoreWorker.h"
#include "stopwords/StopwordFilter.h"
#include "stemmer/ThreadSafeUtf8Stemmer.h"
#include "server/RelevanceServer.h"
#include "models/ProcessedDocument.h"
#include "server/RelevanceServer.h"
#include "models/Centroid.h"
#include "models/Document.h"

#include "util/util.h"
#include "text_util/ScoredWord.h"
#include "util/Clock.h"
#include "util/Sha1Hasher.h"


using namespace std;
using namespace wangle;
using namespace relevanced;
using namespace relevanced::persistence;
using namespace relevanced::models;
using namespace relevanced::util;
using namespace relevanced::text_util;
using namespace relevanced::centroid_update_worker;
using namespace relevanced::document_processing_worker;
using namespace relevanced::similarity_score_worker;
using namespace relevanced::stemmer;
using namespace relevanced::stopwords;
using namespace relevanced::server;
using namespace relevanced::tokenizer;
using namespace relevanced::thrift_protocol;



using ::testing::Return;
using ::testing::_;


struct RelevanceServerTestCtx {
  shared_ptr<PersistenceIf> persistence;
  shared_ptr<CentroidMetadataDbIf> metadb;
  shared_ptr<Sha1HasherIf> hasher;
  shared_ptr<ClockIf> sysClock;
  shared_ptr<StemmerIf> stemmer;
  shared_ptr<StopwordFilterIf> stopwordFilter;
  shared_ptr<DocumentProcessorIf> processor;
  shared_ptr<CentroidUpdaterFactoryIf> updaterFactory;
  shared_ptr<DocumentAccumulatorFactoryIf> accumulatorFactory;
  shared_ptr<FutureExecutor<CPUThreadPoolExecutor>> persistenceThreads;
  shared_ptr<FutureExecutor<CPUThreadPoolExecutor>> processingThreads;
  shared_ptr<FutureExecutor<CPUThreadPoolExecutor>> scoringThreads;
  shared_ptr<FutureExecutor<CPUThreadPoolExecutor>> updatingThreads;
  shared_ptr<SimilarityScoreWorker> scoreWorker;
  shared_ptr<DocumentProcessingWorker> processingWorker;
  shared_ptr<CentroidUpdateWorker> updateWorker;
  shared_ptr<RelevanceServer> server;

  RelevanceServerTestCtx(bool initialize = true) {
    persistenceThreads.reset(new wangle::FutureExecutor<wangle::CPUThreadPoolExecutor>(2));
    processingThreads.reset(new wangle::FutureExecutor<wangle::CPUThreadPoolExecutor>(2));
    scoringThreads.reset(new wangle::FutureExecutor<wangle::CPUThreadPoolExecutor>(2));
    updatingThreads.reset(new wangle::FutureExecutor<wangle::CPUThreadPoolExecutor>(2));
    UniquePointer<RockHandleIf> rockHandle(new InMemoryRockHandle("foo"));
    UniquePointer<SyncPersistenceIf> syncPersistence(
      new SyncPersistence(std::move(rockHandle))
    );
    persistence.reset(new Persistence(std::move(syncPersistence), persistenceThreads));
    hasher.reset(new Sha1Hasher);
    metadb.reset(new CentroidMetadataDb(persistence));
    stemmer.reset(new ThreadSafeUtf8Stemmer);
    stopwordFilter.reset(new StopwordFilter);
    sysClock.reset(new Clock);
    processor.reset(
      new DocumentProcessor(stemmer, stopwordFilter, sysClock)
    );
    accumulatorFactory.reset(new DocumentAccumulatorFactory);
    updaterFactory.reset(new CentroidUpdaterFactory(
      persistence, metadb, accumulatorFactory, sysClock
    ));
    processingWorker.reset(new DocumentProcessingWorker(
      processor, hasher, processingThreads
    ));
    scoreWorker.reset(new SimilarityScoreWorker(
      persistence, metadb, scoringThreads
    ));
    updateWorker.reset(new CentroidUpdateWorker(
      updaterFactory, updatingThreads
    ));
    server.reset(new RelevanceServer(
      persistence, metadb, sysClock, scoreWorker, processingWorker, updateWorker
    ));
    if (initialize) {
      server->initialize();
      updateWorker->debug_getUpdateQueue()->debug_setVeryShortTimeouts();
    }
  }
  ~RelevanceServerTestCtx() {
    updateWorker->join();
  }
};

TEST(RelevanceServer, TestInitialize) {
  bool initialize = false;
  RelevanceServerTestCtx ctx(initialize);
  auto centroid = std::make_shared<Centroid>(
    "some-id", unordered_map<string, double> {{"cats", 0.5}, {"dogs", 0.5}}, 15.3
  );
  ctx.persistence->saveCentroid("some-id", centroid).get();
  ctx.server->initialize();
  auto loaded = ctx.scoreWorker->debugGetCentroid("some-id");
  EXPECT_TRUE(loaded.hasValue());
  EXPECT_EQ("some-id", loaded.value()->id);
}

TEST(RelevanceServer, TestAddDocumentToCentroidHappy) {
  RelevanceServerTestCtx ctx;
  ctx.updateWorker->debug_getUpdateQueue()->debug_setShortTimeouts();
  auto centroid = std::make_shared<Centroid>(
    "centroid-id", unordered_map<string, double> {{"cats", 0.5}, {"dogs", 0.5}}, 15.3
  );
  ctx.persistence->saveCentroid("centroid-id", centroid).get();
  ctx.server->createDocumentWithID(
    folly::make_unique<string>("doc-id"),
    folly::make_unique<string>("some text about dogs")
  ).get();
  auto response = ctx.server->addDocumentToCentroid(
    folly::make_unique<string>("centroid-id"),
    folly::make_unique<string>("doc-id")
  ).get();
  EXPECT_FALSE(response.hasException());
  auto joinResponse = ctx.server->joinCentroid(folly::make_unique<string>("centroid-id")).get();
  EXPECT_FALSE(joinResponse.hasException());
  EXPECT_TRUE(joinResponse.value());
}

TEST(RelevanceServer, TestAddDocumentToCentroidAlreadyInCentroid) {
  RelevanceServerTestCtx ctx;
  auto centroid = std::make_shared<Centroid>(
    "centroid-id", unordered_map<string, double> {{"cats", 0.5}, {"dogs", 0.5}}, 15.3
  );
  ctx.persistence->saveCentroid("centroid-id", centroid).get();
  ctx.server->createDocumentWithID(
    folly::make_unique<string>("doc-id"),
    folly::make_unique<string>("some text about dogs")
  ).get();
  auto response1 = ctx.server->addDocumentToCentroid(
    folly::make_unique<string>("centroid-id"),
    folly::make_unique<string>("doc-id")
  ).get();
  EXPECT_FALSE(response1.hasException());
  auto response2 = ctx.server->addDocumentToCentroid(
    folly::make_unique<string>("centroid-id"),
    folly::make_unique<string>("doc-id")
  ).get();
  EXPECT_TRUE(response2.hasException<EDocumentAlreadyInCentroid>());
}

TEST(RelevanceServer, TestAddDocumentToCentroidMissingDocument) {
  RelevanceServerTestCtx ctx;
  auto centroid = std::make_shared<Centroid>(
    "centroid-id", unordered_map<string, double> {{"cats", 0.5}, {"dogs", 0.5}}, 15.3
  );
  ctx.persistence->saveCentroid("centroid-id", centroid).get();
  ctx.server->createDocumentWithID(
    folly::make_unique<string>("unrelated-doc-id"),
    folly::make_unique<string>("some text about dogs")
  ).get();
  auto response = ctx.server->addDocumentToCentroid(
    folly::make_unique<string>("centroid-id"),
    folly::make_unique<string>("missing-doc-id")
  ).get();
  EXPECT_TRUE(response.hasException<EDocumentDoesNotExist>());
}

TEST(RelevanceServer, TestAddDocumentToCentroidMissingCentroid) {
  RelevanceServerTestCtx ctx;
  auto centroid = std::make_shared<Centroid>(
    "centroid-id", unordered_map<string, double> {{"cats", 0.5}, {"dogs", 0.5}}, 15.3
  );
  ctx.persistence->saveCentroid("centroid-id", centroid).get();
  ctx.server->createDocumentWithID(
    folly::make_unique<string>("doc-id"),
    folly::make_unique<string>("some text about dogs")
  ).get();
  auto response = ctx.server->addDocumentToCentroid(
    folly::make_unique<string>("missing-centroid-id"),
    folly::make_unique<string>("doc-id")
  ).get();
  EXPECT_TRUE(response.hasException<ECentroidDoesNotExist>());
}

TEST(RelevanceServer, TestAddDocumentToCentroidMissingBoth) {
  RelevanceServerTestCtx ctx;
  auto centroid = std::make_shared<Centroid>(
    "centroid-id", unordered_map<string, double> {{"cats", 0.5}, {"dogs", 0.5}}, 15.3
  );
  ctx.persistence->saveCentroid("centroid-id", centroid).get();
  ctx.server->createDocumentWithID(
    folly::make_unique<string>("doc-id"),
    folly::make_unique<string>("some text about dogs")
  ).get();
  auto response = ctx.server->addDocumentToCentroid(
    folly::make_unique<string>("missing-centroid-id"),
    folly::make_unique<string>("missing-doc-id")
  ).get();
  EXPECT_TRUE(response.hasException<ECentroidDoesNotExist>());
}

TEST(RelevanceServer, TestRemoveDocumentFromCentroidHappy) {
  RelevanceServerTestCtx ctx;
  auto centroid = std::make_shared<Centroid>(
    "centroid-id", unordered_map<string, double> {{"cats", 0.5}, {"dogs", 0.5}}, 15.3
  );
  ctx.persistence->saveCentroid("centroid-id", centroid).get();
  ctx.server->createDocumentWithID(
    folly::make_unique<string>("doc-id"),
    folly::make_unique<string>("some text about dogs")
  ).get();
  auto response1 = ctx.server->addDocumentToCentroid(
    folly::make_unique<string>("centroid-id"),
    folly::make_unique<string>("doc-id")
  ).get();
  EXPECT_FALSE(response1.hasException());
  auto response2 = ctx.server->removeDocumentFromCentroid(
    folly::make_unique<string>("centroid-id"),
    folly::make_unique<string>("doc-id")
  ).get();
  EXPECT_FALSE(response2.hasException());
}

TEST(RelevanceServer, TestRemoveDocumentFromCentroidDocumentNotInCentroid) {
  RelevanceServerTestCtx ctx;
  auto centroid = std::make_shared<Centroid>(
    "centroid-id", unordered_map<string, double> {{"cats", 0.5}, {"dogs", 0.5}}, 15.3
  );
  ctx.persistence->saveCentroid("centroid-id", centroid).get();
  ctx.server->createDocumentWithID(
    folly::make_unique<string>("doc-id"),
    folly::make_unique<string>("some text about dogs")
  ).get();
  auto response = ctx.server->removeDocumentFromCentroid(
    folly::make_unique<string>("centroid-id"),
    folly::make_unique<string>("doc-id")
  ).get();
  EXPECT_TRUE(response.hasException<EDocumentNotInCentroid>());
}

TEST(RelevanceServer, TestRemoveDocumentFromCentroidMissingDocument) {
  RelevanceServerTestCtx ctx;
  auto centroid = std::make_shared<Centroid>(
    "centroid-id", unordered_map<string, double> {{"cats", 0.5}, {"dogs", 0.5}}, 15.3
  );
  ctx.persistence->saveCentroid("centroid-id", centroid).get();
  auto response = ctx.server->removeDocumentFromCentroid(
    folly::make_unique<string>("centroid-id"),
    folly::make_unique<string>("missing-doc-id")
  ).get();
  EXPECT_TRUE(response.hasException<EDocumentDoesNotExist>());
}

TEST(RelevanceServer, TestRemoveDocumentFromCentroidMissingCentroid) {
  RelevanceServerTestCtx ctx;
  auto centroid = std::make_shared<Centroid>(
    "centroid-id", unordered_map<string, double> {{"cats", 0.5}, {"dogs", 0.5}}, 15.3
  );
  ctx.persistence->saveCentroid("centroid-id", centroid).get();
  ctx.server->createDocumentWithID(
    folly::make_unique<string>("doc-id"),
    folly::make_unique<string>("some text about dogs")
  ).get();
  auto response = ctx.server->removeDocumentFromCentroid(
    folly::make_unique<string>("missing-centroid-id"),
    folly::make_unique<string>("doc-id")
  ).get();
  EXPECT_TRUE(response.hasException<ECentroidDoesNotExist>());
}

TEST(RelevanceServer, TestRemoveDocumentFromCentroidMissingBoth) {
  RelevanceServerTestCtx ctx;
  auto centroid = std::make_shared<Centroid>(
    "centroid-id", unordered_map<string, double> {{"cats", 0.5}, {"dogs", 0.5}}, 15.3
  );
  ctx.persistence->saveCentroid("centroid-id", centroid).get();
  ctx.server->createDocumentWithID(
    folly::make_unique<string>("doc-id"),
    folly::make_unique<string>("some text about dogs")
  ).get();
  auto response = ctx.server->removeDocumentFromCentroid(
    folly::make_unique<string>("missing-centroid-id"),
    folly::make_unique<string>("missing-doc-id")
  ).get();
  EXPECT_TRUE(response.hasException<ECentroidDoesNotExist>());
}

TEST(RelevanceServer, TestCreateDocument) {
  RelevanceServerTestCtx ctx;
  auto text = folly::make_unique<string>("some text about cats and dogs and fish and so forth");

  auto response = ctx.server->createDocument(std::move(text)).get();
  EXPECT_TRUE(response.hasValue());
  string docId = *response.value();
  auto persisted = ctx.persistence->loadDocumentOption(docId).get();
  EXPECT_TRUE(persisted.hasValue());
  EXPECT_EQ(docId, persisted.value()->id);
}

TEST(RelevanceServer, TestCreateDocumentWithID) {
  RelevanceServerTestCtx ctx;
  auto text = folly::make_unique<string>("some text about cats and dogs and fish and so forth");
  auto id = folly::make_unique<string>("doc-id");
  auto response = ctx.server->createDocumentWithID(std::move(id), std::move(text)).get();
  EXPECT_TRUE(response.hasValue());
  EXPECT_EQ("doc-id", *response.value());
  auto persisted = ctx.persistence->loadDocumentOption("doc-id").get();
  EXPECT_TRUE(persisted.hasValue());
  EXPECT_EQ("doc-id", persisted.value()->id);
}

TEST(RelevanceServer, TestCreateDocumentWithIDAlreadyExists) {
  RelevanceServerTestCtx ctx;
  auto text = folly::make_unique<string>("some text about cats and dogs and fish and so forth");
  auto id = folly::make_unique<string>("doc-id");
  auto response1 = ctx.server->createDocumentWithID(std::move(id), std::move(text)).get();
  EXPECT_FALSE(response1.hasException());
  auto response2 = ctx.server->createDocumentWithID(
    folly::make_unique<string>("doc-id"),
    folly::make_unique<string>("some text")
  ).get();
  EXPECT_TRUE(response2.hasException<EDocumentAlreadyExists>());
}

TEST(RelevanceServer, TestCreateCentroid) {
  RelevanceServerTestCtx ctx;
  auto id = folly::make_unique<string>("some-centroid");
  auto response = ctx.server->createCentroid(std::move(id)).get();
  EXPECT_TRUE(response.hasValue());
  EXPECT_TRUE(response.value());
  auto persisted = ctx.persistence->loadCentroidOption("some-centroid").get();
  EXPECT_TRUE(persisted.hasValue());
  EXPECT_EQ("some-centroid", persisted.value()->id);
}

TEST(RelevanceServer, TestCreateCentroidAlreadyExists) {
  RelevanceServerTestCtx ctx;
  auto id = folly::make_unique<string>("some-centroid");
  auto response1 = ctx.server->createCentroid(folly::make_unique<string>("some-centroid")).get();
  EXPECT_FALSE(response1.hasException());
  auto response2 = ctx.server->createCentroid(folly::make_unique<string>("some-centroid")).get();
  EXPECT_TRUE(response2.hasException<ECentroidAlreadyExists>());
}

TEST(RelevanceServer, TestListAllCentroids) {
  RelevanceServerTestCtx ctx;
  vector<Future<Try<bool>>> creations;
  set<string> expectedIds;
  for (size_t i = 0; i < 10; i++) {
    auto id = sformat("some-centroid-{}", i);
    expectedIds.insert(id);
    creations.push_back(ctx.server->createCentroid(
      folly::make_unique<string>(id)
    ));
  }
  collect(creations).get();
  auto centroids = ctx.server->listAllCentroids().get();
  set<string> returnedIds;
  for (auto &id: *centroids) {
    returnedIds.insert(id);
  }
  EXPECT_EQ(expectedIds, returnedIds);
}

TEST(RelevanceServer, TestListCentroidRange) {
  RelevanceServerTestCtx ctx;
  vector<Future<Try<bool>>> creations;
  for (size_t i = 0; i < 10; i++) {
    auto id = sformat("some-centroid-{}", i);
    creations.push_back(ctx.server->createCentroid(
      folly::make_unique<string>(id)
    ));
  }
  collect(creations).get();
  auto centroids = ctx.server->listCentroidRange(3, 4).get();
  set<string> returnedIds;
  for (auto &id: *centroids) {
    returnedIds.insert(id);
  }
  set<string> expectedIds {
    "some-centroid-3", "some-centroid-4", "some-centroid-5", "some-centroid-6"
  };
  EXPECT_EQ(expectedIds, returnedIds);
}

TEST(RelevanceServer, TestListCentroidRangeFromID) {
  RelevanceServerTestCtx ctx;
  vector<Future<Try<bool>>> creations;
  for (size_t i = 0; i < 10; i++) {
    auto id = sformat("some-centroid-{}", i);
    creations.push_back(ctx.server->createCentroid(
      folly::make_unique<string>(id)
    ));
  }
  collect(creations).get();
  auto centroids = ctx.server->listCentroidRangeFromID(
    folly::make_unique<string>("some-centroid-5"), 3
  ).get();
  set<string> returnedIds;
  for (auto &id: *centroids) {
    returnedIds.insert(id);
  }
  set<string> expectedIds {
    "some-centroid-5", "some-centroid-6", "some-centroid-7"
  };
  EXPECT_EQ(expectedIds, returnedIds);
}

TEST(RelevanceServer, TestListAllDocuments) {
  RelevanceServerTestCtx ctx;
  vector<Future<Try<unique_ptr<string>>>> creations;
  set<string> expectedIds;
  for (size_t i = 0; i < 10; i++) {
    auto id = sformat("some-doc-{}", i);
    expectedIds.insert(id);
    creations.push_back(ctx.server->createDocumentWithID(
      folly::make_unique<string>(id),
      folly::make_unique<string>("this is some text about things")
    ));
  }
  set<string> createdIds;
  auto created = collect(creations).get();
  for (auto &id: created) {
    EXPECT_TRUE(id.hasValue());
    createdIds.insert(*id.value());
  }
  EXPECT_EQ(expectedIds, createdIds);

  auto documents = ctx.server->listAllDocuments().get();
  set<string> returnedIds;
  for (auto &id: *documents) {
    returnedIds.insert(id);
  }
  EXPECT_EQ(expectedIds, returnedIds);
}

TEST(RelevanceServer, TestListDocumentRange) {
  RelevanceServerTestCtx ctx;
  vector<Future<Try<unique_ptr<string>>>> creations;
  for (size_t i = 0; i < 10; i++) {
    auto id = sformat("some-doc-{}", i);
    creations.push_back(ctx.server->createDocumentWithID(
      folly::make_unique<string>(id),
      folly::make_unique<string>("this is some text about things")
    ));
  }
  collect(creations).get();
  auto documents = ctx.server->listDocumentRange(3, 4).get();
  set<string> returnedIds;
  for (auto &id: *documents) {
    returnedIds.insert(id);
  }
  set<string> expectedIds {
    "some-doc-3", "some-doc-4", "some-doc-5", "some-doc-6"
  };
  EXPECT_EQ(expectedIds, returnedIds);
}

TEST(RelevanceServer, TestListDocumentRangeFromID) {
  RelevanceServerTestCtx ctx;
  vector<Future<Try<unique_ptr<string>>>> creations;
  for (size_t i = 0; i < 10; i++) {
    auto id = sformat("some-doc-{}", i);
    creations.push_back(ctx.server->createDocumentWithID(
      folly::make_unique<string>(id),
      folly::make_unique<string>("this is some text about things")
    ));
  }
  collect(creations).get();
  auto documents = ctx.server->listDocumentRangeFromID(
    folly::make_unique<string>("some-doc-4"), 3
  ).get();
  set<string> returnedIds;
  for (auto &id: *documents) {
    returnedIds.insert(id);
  }
  set<string> expectedIds {
    "some-doc-4", "some-doc-5", "some-doc-6"
  };
  EXPECT_EQ(expectedIds, returnedIds);
}

TEST(RelevanceServer, TestDeleteDocument) {
  RelevanceServerTestCtx ctx;
  vector<Future<Try<unique_ptr<string>>>> creations;
  for (size_t i = 0; i < 6; i++) {
    auto id = sformat("some-doc-{}", i);
    creations.push_back(ctx.server->createDocumentWithID(
      folly::make_unique<string>(id),
      folly::make_unique<string>("this is some text about things")
    ));
  }
  collect(creations).get();
  ctx.server->deleteDocument(folly::make_unique<string>("some-doc-3")).get();
  ctx.server->deleteDocument(folly::make_unique<string>("some-doc-4")).get();
  set<string> expectedIds {
    "some-doc-0", "some-doc-1", "some-doc-2", "some-doc-5"
  };
  auto documents = ctx.server->listAllDocuments().get();
  set<string> returnedIds;
  for (auto &id: *documents) {
    returnedIds.insert(id);
  }
  EXPECT_EQ(expectedIds, returnedIds);
}

TEST(RelevanceServer, TestDeleteDocumentMissing) {
  RelevanceServerTestCtx ctx;
  vector<Future<Try<unique_ptr<string>>>> creations;
  for (size_t i = 0; i < 6; i++) {
    auto id = sformat("some-doc-{}", i);
    creations.push_back(ctx.server->createDocumentWithID(
      folly::make_unique<string>(id),
      folly::make_unique<string>("this is some text about things")
    ));
  }
  collect(creations).get();
  auto result = ctx.server->deleteDocument(folly::make_unique<string>("some-doc-8")).get();
  EXPECT_TRUE(result.hasException<EDocumentDoesNotExist>());
}
