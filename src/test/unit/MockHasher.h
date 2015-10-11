#pragma once
#include <string>
#include "util/Sha1Hasher.h"
#include "gmock/gmock.h"

using namespace std;

class MockHasher: public relevanced::util::Sha1HasherIf {
public:
  MOCK_METHOD1(hash, string(const string&));
  MOCK_METHOD1(hash, string(string*));
};
