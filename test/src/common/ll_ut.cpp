#include "gtest/gtest.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ll.h"

#define MIN_NODES UINT8_MAX
#define MAX_NODES 8192
#define TRAVERSE_CB_VAL UINT16_MAX
#define TRAVERSE_COUNT 10
#define TRAVERSE_WRITE_DATA 0xBF

typedef int (*LLTestCb_t)(LL_t *ll,
                          LLNode_t node);
static LLNode_t *nodes;
static uint8_t *origin;
static size_t nCount;
static size_t bSize;
static uint32_t noReach;

// Generate a sequence of random unique numbers
// see: https://preshing.com/20121224/how-to-generate-a-sequence-of-unique-random-integers/
class RandomSequenceOfUnique
{
private:
  uint32_t i;
  uint32_t intermediate;

  static uint32_t qpr(uint32_t x) {
    static const uint32_t prime = 4294967291;
    if (x >= prime) {
      return x;
    }
    uint32_t residue = ((uint64_t) x * x) % prime;
    return (x <= prime / 2) ? residue : prime - residue;
  }

public:
  RandomSequenceOfUnique(uint32_t base, uint32_t offset) {
    i = qpr(qpr(base) + 0x682f0161);
    intermediate = qpr(qpr(offset) + 0x46790905);
  }

  unsigned int next() {
    return qpr((qpr(i++) + intermediate) ^ 0x5bf03635);
  }
};

class LLTest : public ::testing::Test {
 protected:

  LLTest() {
  }

  ~LLTest() override {
  }

  void SetUp() override {
    srand(time(NULL));
    bSize = 0;
    nCount = 0;
    if (nodes) {
      nodes = NULL;
    }
    if (origin) {
      origin = NULL;
    }
  }

  void TearDown() override {
    if (nodes) {
      free(nodes);
      nodes = NULL;
    }
    if (origin) {
      free(origin);
      origin = NULL;
    }
  }

  size_t RndInt(size_t mod, size_t min) {
    size_t ret = 0;
    ret = rand() % mod;
    ret = ret < min ? min : ret;
    return ret;
  }
  void RndArray(uint8_t *buf, size_t size) {
    for (size_t i = 0U; i < size; i++) {
        buf[i] = rand() % UINT8_MAX;
    }
  }

  LL_t CreateTest(LLTestCb_t cb) {
    nCount = RndInt(MIN_NODES, MAX_NODES);
    bSize = RndInt(UINT8_MAX, UINT8_MAX / 2);
    LL_t ll = {NULL, NULL, NULL};
    nodes = (LLNode_t *) calloc(nCount, sizeof(LLNode_t));
    uint8_t *data = (uint8_t *) calloc(nCount * bSize, sizeof(uint8_t));
    origin = data;
    uint32_t seed = time(NULL);
    RandomSequenceOfUnique rsu(seed, seed + 1);

    // Add random data to linked list node pointer and push to instance
    for (size_t i = 0; i < nCount; i++) {
      nodes[i].id = rsu.next();
      RndArray(data, bSize);
      nodes[i].data = data;
      data += bSize;
      EXPECT_EQ(LLNodeAdd(&ll, &nodes[i]), 0);
    }

    // Set a node id that is not reachable
    noReach = rsu.next();

    // Run callback in random order (Fisher-Yates shuffle)
    // with a copy of Linked List to avoid changing original addresses
    if (cb) {
      LLNode_t *copy = (LLNode_t *) calloc(nCount, sizeof(LLNode_t));
      memcpy(copy, nodes, sizeof(LLNode_t) * nCount);
      for (size_t i = 0; i < nCount - 1; i++) {
        size_t j = RndInt(nCount - 1, i);
        LLNode_t temp;
        temp = copy[i];
        copy[i] = copy[j];
        copy[j] = temp;
        EXPECT_EQ(cb(&ll, copy[i]), 0);
      }
      free(copy);
    }
    return ll;
  }
};

TEST_F(LLTest, Add) {
  LL_t ll = CreateTest(NULL);
  LLNode_t node;
  // Test improper use case
  EXPECT_NE(LLNodeAdd(NULL, NULL), 0);
  EXPECT_NE(LLNodeAdd(&ll, NULL), 0);
  EXPECT_NE(LLNodeAdd(NULL, &node), 0);
}

static int LLGetTest(LL_t *ll,
                     LLNode_t node) {
  int ret = 0;
  uint8_t *cmp = NULL;
  EXPECT_EQ(LLGetElement(ll, node.id, (void **) &cmp), 0);
  EXPECT_NE(cmp, nullptr);
  if (cmp)
  {
      EXPECT_EQ(memcmp(cmp, node.data, bSize), 0);
  }
  return ret;
}

TEST_F(LLTest, Get) {
  LL_t ll = CreateTest(LLGetTest);
  void *cmp = NULL;
  // Test improper use case
  EXPECT_NE(LLGetElement(NULL, 0, &cmp), 0);
  EXPECT_NE(LLGetElement(&ll, 0, NULL), 0);
  EXPECT_NE(LLGetElement(&ll, noReach, &cmp), 0);
}

static int LLRemoveTest(LL_t *ll,
                        LLNode_t node) {
  int ret = 0;
  uint8_t *data = NULL;
  EXPECT_EQ(LLRemove(ll, node.id), 0);
  // Test we can no longer find this node
  EXPECT_NE(LLRemove(ll, node.id), 0);
  EXPECT_NE(LLGetElement(ll, node.id, (void **) &data), 0);
  return ret;
}

TEST_F(LLTest, Remove) {
  CreateTest(LLRemoveTest);
  // Test improper use case
  ASSERT_NE(LLRemove(NULL, 0), 0);
}

static int LLTraverseTest(void *data, void *arg) {
  static size_t c = 0;
  uint8_t *cmp = (uint8_t *) data;
  EXPECT_EQ(*(uint32_t *) arg, TRAVERSE_CB_VAL);
  EXPECT_EQ(memcmp(cmp, nodes[c++ % nCount].data, bSize), 0);
  memset(cmp, TRAVERSE_WRITE_DATA, bSize);
  return 0;
}

TEST_F(LLTest, Traverse) {
  LL_t ll = CreateTest(NULL);
  uint32_t arg = TRAVERSE_CB_VAL;
  for (size_t i = 0; i < TRAVERSE_COUNT; i++) {
    LLTraverse(&ll, LLTraverseTest, &arg);
  }
  // Test accessed variable in callback is properly manipulated
  for (size_t i = 0; i < nCount * bSize; i++)
  {
    EXPECT_EQ(origin[i], TRAVERSE_WRITE_DATA);
  }
  // Test improper use cases
  ASSERT_NE(LLTraverse(NULL, NULL, &arg), 0);
  ASSERT_NE(LLTraverse(&ll, NULL, &arg), 0);
  ASSERT_NE(LLTraverse(NULL, LLTraverseTest, &arg), 0);
}
