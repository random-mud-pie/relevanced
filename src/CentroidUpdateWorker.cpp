#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <thread>
#include <chrono>
#include <wangle/concurrent/CPUThreadPoolExecutor.h>
#include <wangle/concurrent/FutureExecutor.h>
#include <folly/futures/Future.h>
#include "CentroidUpdateWorker.h"
#include "CentroidUpdaterFactory.h"
#include "persistence/Persistence.h"
#include "Debouncer.h"
using persistence::PersistenceIf;
using namespace std;
using namespace folly;
using namespace wangle;

CentroidUpdateWorker::CentroidUpdateWorker(
  shared_ptr<CentroidUpdaterFactoryIf> updaterFactory,
  shared_ptr<FutureExecutor<CPUThreadPoolExecutor>> threadPool
): updaterFactory_(updaterFactory), threadPool_(threadPool) {}

void CentroidUpdateWorker::initialize() {
  chrono::milliseconds initialDelay(5000);
  chrono::milliseconds debounceInterval(30000);
  updateQueue_ = make_shared<Debouncer<string>>(
    initialDelay, debounceInterval, [this](string centroidId) {
      update(centroidId);
    }
  );
}

void CentroidUpdateWorker::stop() {
  stopping_ = true;
  updateQueue_->stop();
}

Future<bool> CentroidUpdateWorker::update(const string &centroidId) {
  return threadPool_->addFuture([this, centroidId](){
    auto updater = updaterFactory_->makeForCentroidId(centroidId);
    bool result = updater->run();
    makeFuture(centroidId).delayed(chrono::milliseconds(50)).then([this](string centroidId) {
      this->echoUpdated(centroidId);
    });
    return result;
  });
}

void CentroidUpdateWorker::echoUpdated(const string &centroidId) {
  SYNCHRONIZED(updateCallbacks_) {
    for (auto &cb: updateCallbacks_) {
      cb(centroidId);
    }
  }
}

void CentroidUpdateWorker::onUpdate(function<void (const string&)> callback) {
  updateCallbacks_->push_back(std::move(callback));
}

void CentroidUpdateWorker::triggerUpdate(const string &centroidId) {
  string toWrite = centroidId;
  updateQueue_->write(toWrite);
}
