#include "gtest/gtest.h"

#include "fff.h"
#include "mock_uptime.h"
extern "C" {
#include "mock_FreeRTOS.h"
}

DEFINE_FFF_GLOBALS;

#include "sensorSampler.h"
#include "sensorSampler.cpp"

#define SENSOR_SAMPLE_PERIOD_MS (10000)
#define MAX_VALUE_SENSOR_POLL (1 << (MAX_SENSORS - 1))

typedef void (*SensorSampleTestCb_t)(char *name);

static struct count_s
{
    size_t init;
    size_t poll;
    size_t check;
} count;

static struct
{
    uint32_t samples;
    BaseType_t ret;
    bool check;
} task;

static bool testInit(void) {
  count.init++;
  return true;
}

static bool testPoll(void) {
  count.poll++;
  return true;
}

static bool testCheck(void) {
  count.check++;
  return task.check;
}

class SensorSampleTest : public ::testing::Test {
 protected:
  char *names;
  sensorNode_t *node;
  size_t tSize;
  size_t nCount;

  SensorSampleTest() {
  }

  ~SensorSampleTest() override {
  }

  void SetUp() override {
    static sensorConfig_t cfg = SENSOR_DEFAULT_CONFIG;
    char temp[] = "TEST000";
    srand(time(NULL));
    RESET_FAKE(xTaskCreate);
    RESET_FAKE(xTimerCreate);
    RESET_FAKE(xTimerGenericCommand);
    RESET_FAKE(xTimerIsTimerActive);
    count = {0, 0, 0};
    names = NULL;
    tSize = strlen(temp) + 1;
    xTaskCreate_fake.return_val = pdTRUE;
    sensorSamplerInit(&cfg);
    EXPECT_EQ(xTaskCreate_fake.call_count, 1);
  }

  void TearDown() override {
    if (names) {
      free(names);
    }
    count = {0, 0, 0};
    sensorSamplerDeinit();
  }

  size_t RndInt(size_t mod, size_t min) {
    size_t ret = 0;
    ret = rand() % mod;
    ret = ret < min ? min : ret;
    return ret;
  }
  void AddNodes(uint8_t num) {
    nCount = num = num > MAX_SENSORS ? MAX_SENSORS : num;
    node = (sensorNode_t *) calloc(num, sizeof(sensorNode_t));
    names = (char *) calloc(tSize * num, sizeof(char));
    char *itr = names;
    RESET_FAKE(xTimerCreate);

    for (uint8_t i = 0; i < num; i++) {
      node[i] = SENSOR_NODE_INIT(testInit, testPoll, testCheck);
      snprintf(itr, tSize, "TEST%03d", i);
      xTimerCreate_fake.return_val = (TimerHandle_t) RndInt(UINT16_MAX, 1);
      xTimerGenericCommand_fake.return_val = pdTRUE;
      EXPECT_EQ(sensorSamplerAdd(&node[i], itr), true);
      EXPECT_EQ(xTimerCreate_fake.arg3_val, &node[i].list.flag);
      itr += tSize;
    }
    EXPECT_EQ(count.init, num);
    EXPECT_EQ(xTimerCreate_fake.call_count, num);
  }
  void HandleNodes(SensorSampleTestCb_t cb) {
    char *itr = names;
    for (size_t i = 0; i < nCount; i++) {
      if (cb) {
        cb(itr);
        itr += tSize;
      }
    }
  }
  uint32_t CountSetBits(uint32_t n)
{
  uint32_t count = 0;
  while (n) {
    count += n & 1;
    n >>= 1;
  }
  return count;
}
};

TEST_F(SensorSampleTest, Init) {
  // Test failure cases of initialization
  sensorConfig_t cfg = SENSOR_DEFAULT_CONFIG;
  RESET_FAKE(xTaskCreate);
  xTaskCreate_fake.return_val = pdTRUE;
  EXPECT_DEATH(sensorSamplerInit(NULL), "");
  EXPECT_EQ(xTaskCreate_fake.call_count, 0);
  RESET_FAKE(xTaskCreate);
  xTaskCreate_fake.return_val = pdFALSE;
  EXPECT_DEATH(sensorSamplerInit(&cfg), "");
  // Ensure that a timer is not started if sensor check is set to 0
  RESET_FAKE(xTaskCreate);
  RESET_FAKE(xTimerCreate);
  cfg = {0, 0};
  xTaskCreate_fake.return_val = pdTRUE;
  sensorSamplerInit(&cfg);
  sensorSampleTaskSetup();
  EXPECT_EQ(xTimerCreate_fake.call_count, 0);
}

TEST_F(SensorSampleTest, Add) {
  // Test error conditions
  sensorNode_t node = SENSOR_NODE_INIT(testInit, testPoll, testCheck);
  EXPECT_DEATH(sensorSamplerAdd(NULL, "TEST1"), "");
  EXPECT_DEATH(sensorSamplerAdd(&node, NULL), "");
  node = (sensorNode_t) SENSOR_NODE_INIT(NULL, testPoll, testCheck);
  EXPECT_DEATH(sensorSamplerAdd(&node, "TEST1"), "");
  node = (sensorNode_t) SENSOR_NODE_INIT(testInit, NULL, testCheck);
  EXPECT_DEATH(sensorSamplerAdd(&node, "TEST1"), "");

  AddNodes(MAX_SENSORS);
  EXPECT_EQ(count.init, MAX_SENSORS);
}

static void DisableCallback(char *name)
{
  // If timer is running return true
  xTimerIsTimerActive_fake.return_val = pdTRUE;
  EXPECT_EQ(sensorSamplerDisable(name), true);
  // If timer is not running return false
  xTimerIsTimerActive_fake.return_val = pdFALSE;
  EXPECT_EQ(sensorSamplerDisable(name), false);
}

TEST_F(SensorSampleTest, Disable) {
  AddNodes(MAX_SENSORS);
  HandleNodes(DisableCallback);
  EXPECT_DEATH(sensorSamplerDisable(NULL), "");
}

static void EnableCallback(char *name)
{
  // If timer is not running return true
  xTimerIsTimerActive_fake.return_val = pdFALSE;
  EXPECT_EQ(sensorSamplerEnable(name), true);
  // If timer is running return false
  xTimerIsTimerActive_fake.return_val = pdTRUE;
  EXPECT_EQ(sensorSamplerEnable(name), false);
}

TEST_F(SensorSampleTest, Enable) {
  AddNodes(MAX_SENSORS);
  HandleNodes(EnableCallback);
  EXPECT_DEATH(sensorSamplerEnable(NULL), "");
}

static void SamplePeriodCb(char *name)
{
  xTimerIsTimerActive_fake.return_val = pdTRUE;
  EXPECT_EQ(sensorSamplerChangeSamplingPeriodMs(name, SENSOR_SAMPLE_PERIOD_MS), true);
  // This expect death call takes a lot of time to run for all nodes
  xTimerGenericCommand_fake.return_val = pdFAIL;
  EXPECT_DEATH(sensorSamplerChangeSamplingPeriodMs(name, SENSOR_SAMPLE_PERIOD_MS), "");
  xTimerGenericCommand_fake.return_val = pdTRUE;
  xTimerGetPeriod_fake.return_val = SENSOR_SAMPLE_PERIOD_MS / (1000 / configTICK_RATE_HZ);
  EXPECT_EQ(sensorSamplerGetSamplingPeriodMs(name), SENSOR_SAMPLE_PERIOD_MS);
}

TEST_F(SensorSampleTest, SamplePeriod) {
  AddNodes(MAX_SENSORS);
  HandleNodes(SamplePeriodCb);
  EXPECT_DEATH(sensorSamplerChangeSamplingPeriodMs(NULL, SENSOR_SAMPLE_PERIOD_MS), "");
  EXPECT_DEATH(sensorSamplerGetSamplingPeriodMs(NULL), "");
}

TEST_F(SensorSampleTest, Checks) {
  xTimerCreate_fake.return_val = (TimerHandle_t) UINT32_MAX;
  sensorSampleTaskSetup();
  xTimerIsTimerActive_fake.return_val = pdTRUE;
  xTimerGenericCommand_fake.return_val = pdTRUE;
  EXPECT_EQ(sensorSamplerDisableChecks(), true);
  xTimerIsTimerActive_fake.return_val = pdFALSE;
  EXPECT_EQ(sensorSamplerDisableChecks(), false);
  xTimerIsTimerActive_fake.return_val = pdFALSE;
  xTimerGenericCommand_fake.return_val = pdTRUE;
  EXPECT_EQ(sensorSamplerEnableChecks(), true);
  xTimerIsTimerActive_fake.return_val = pdTRUE;
  EXPECT_EQ(sensorSamplerEnableChecks(), false);
}

static BaseType_t xTaskNotifyWait_Custom(UBaseType_t uxIndexToWaitOn,
      uint32_t ulBitsToClearOnEntry,
      uint32_t ulBitsToClearOnExit,
      uint32_t *bits,
      TickType_t xTicksToWait) {
  (void) uxIndexToWaitOn;
  (void) ulBitsToClearOnEntry;
  (void) ulBitsToClearOnExit;
  (void) xTicksToWait;
  *bits = task.samples;
  return task.ret;
}

static int timerClearTraverse(void *data, void *arg) {
  sensorListItem_t *item = NULL;
  (void) arg;
  if (data) {
    item = (sensorListItem_t *) data;
    item->timer = NULL;
  }
  return 0;
}

static int checkFnClearTraverse(void *data, void *arg) {
  sensorListItem_t *item = NULL;
  (void) arg;
  if (data) {
    item = (sensorListItem_t *) data;
    item->sensor.checkFn = NULL;
  }
  return 0;
}

TEST_F(SensorSampleTest, NotifySamples) {
  uint32_t loops = RndInt(8192, 1024);
  xTaskGenericNotifyWait_fake.custom_fake = xTaskNotifyWait_Custom;
  AddNodes(MAX_SENSORS);
  task.ret = pdTRUE;

  for (uint32_t i = 0; i < loops; i++) {
    count.poll = 0;
    task.samples = RndInt(MAX_VALUE_SENSOR_POLL, 0);
    sensorSampleTaskLoop();
    EXPECT_EQ(count.poll, CountSetBits(task.samples));
  }

  // Bad return on xTaskNotifyWait
  task.check = true;
  count.poll = 0;
  task.samples = 0;
  task.ret = pdFALSE;
  sensorSampleTaskLoop();
  EXPECT_EQ(count.poll, 0);

  // Test check sensors
  task.samples = (1 << MAX_SENSORS);
  task.ret = pdTRUE;
  // Ensure if all sensors are working properly there is no need to re-enable
  xTimerIsTimerActive_fake.return_val = pdTRUE;
  sensorSampleTaskLoop();
  EXPECT_EQ(count.check, MAX_SENSORS);
  count.check = 0;

  // Tests if timer's have stopped and need to be re-enabled
  RESET_FAKE(xTimerIsTimerActive);
  RESET_FAKE(xTimerGenericCommand);
  xTimerIsTimerActive_fake.return_val = pdFALSE;
  sensorSampleTaskLoop();
  EXPECT_EQ(count.check, MAX_SENSORS);
  EXPECT_EQ(xTimerGenericCommand_fake.call_count, MAX_SENSORS);

  // Tests failed checks and disabling timer
  count.check = 0;
  task.check = false;
  RESET_FAKE(xTimerIsTimerActive);
  RESET_FAKE(xTimerGenericCommand);
  xTimerIsTimerActive_fake.return_val = pdTRUE;
  sensorSampleTaskLoop();
  EXPECT_EQ(count.check, MAX_SENSORS);
  EXPECT_EQ(xTimerGenericCommand_fake.call_count, MAX_SENSORS);

  // Test NULL timer items that must be re-init
  count.check = 0;
  LLTraverse(&ll, timerClearTraverse, NULL);
  RESET_FAKE(xTimerIsTimerActive);
  RESET_FAKE(xTimerGenericCommand);
  RESET_FAKE(xTimerCreate);
  xTimerGenericCommand_fake.return_val = pdTRUE;
  xTimerCreate_fake.return_val = (TimerHandle_t) RndInt(UINT16_MAX, 1);
  sensorSampleTaskLoop();
  EXPECT_EQ(count.check, 0);
  EXPECT_EQ(xTimerCreate_fake.call_count, MAX_SENSORS);

  // Test NULL check functions
  LLTraverse(&ll, checkFnClearTraverse, NULL);
  sensorSampleTaskLoop();
  EXPECT_EQ(count.check, 0);
}
