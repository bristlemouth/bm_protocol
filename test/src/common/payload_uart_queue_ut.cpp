#include "FreeRTOS.h"
#include "queue.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <cstring>

// Mock the LPUART1_LINE_BUFF_LEN constant
#define LPUART1_LINE_BUFF_LEN 2048
#define LINE_QUEUE_DEPTH 8

// Structure to hold a single line in the queue (matches payload_uart.cpp)
typedef struct {
  uint8_t buffer[LPUART1_LINE_BUFF_LEN];
  uint16_t len;
} QueuedLine_t;

// Test fixture for PLUART Queue functionality
class PLUARTQueueTest : public ::testing::Test {
protected:
  QueueHandle_t test_queue;
  uint32_t dropped_lines;

  PLUARTQueueTest() : test_queue(NULL), dropped_lines(0) {}

  void SetUp() override {
    // Create a queue similar to the one in payload_uart.cpp
    test_queue = xQueueCreate(LINE_QUEUE_DEPTH, sizeof(QueuedLine_t));
    ASSERT_NE(test_queue, nullptr) << "Failed to create test queue";
    dropped_lines = 0;
  }

  void TearDown() override {
    if (test_queue) {
      vQueueDelete(test_queue);
      test_queue = NULL;
    }
  }

  // Helper function to simulate processLine() behavior
  bool enqueueLine(const char *line_data, size_t len) {
    QueuedLine_t queued_line;
    memcpy(queued_line.buffer, line_data, len);
    queued_line.len = len;

    if (xQueueSend(test_queue, &queued_line, 0) != pdTRUE) {
      // Queue is full - increment dropped line counter
      dropped_lines = dropped_lines + 1;
      return false;
    }
    return true;
  }

  // Helper function to simulate readLine() behavior
  uint16_t dequeueLine(char *buffer, size_t len) {
    QueuedLine_t queued_line;

    if (xQueueReceive(test_queue, &queued_line, 0) == pdTRUE) {
      size_t copy_len;
      if (len > queued_line.len) {
        copy_len = queued_line.len;
      } else {
        copy_len = len;
      }
      memcpy(buffer, queued_line.buffer, copy_len);
      return copy_len;
    }

    return 0; // No line available
  }

  // Helper function to simulate lineAvailable() behavior
  bool lineAvailable() { return (uxQueueMessagesWaiting(test_queue) > 0); }
};

// Test 1: Basic enqueue and dequeue
TEST_F(PLUARTQueueTest, BasicEnqueueDequeue) {
  const char *test_line = "TEST_LINE_1\n";
  char read_buffer[100];

  // Enqueue a line
  EXPECT_TRUE(enqueueLine(test_line, strlen(test_line)));

  // Check line is available
  EXPECT_TRUE(lineAvailable());

  // Dequeue and verify
  uint16_t read_len = dequeueLine(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(read_len, strlen(test_line));
  EXPECT_EQ(memcmp(read_buffer, test_line, read_len), 0);

  // Queue should now be empty
  EXPECT_FALSE(lineAvailable());
}

// Test 2: Multiple lines FIFO ordering
TEST_F(PLUARTQueueTest, FIFOOrdering) {
  const char *lines[] = {"LINE_1\n", "LINE_2\n", "LINE_3\n", "LINE_4\n"};
  char read_buffer[100];

  // Enqueue multiple lines
  for (int i = 0; i < 4; i++) {
    EXPECT_TRUE(enqueueLine(lines[i], strlen(lines[i])));
  }

  // Verify FIFO ordering
  for (int i = 0; i < 4; i++) {
    EXPECT_TRUE(lineAvailable());
    uint16_t read_len = dequeueLine(read_buffer, sizeof(read_buffer));
    EXPECT_EQ(read_len, strlen(lines[i]));
    EXPECT_EQ(memcmp(read_buffer, lines[i], read_len), 0);
  }

  EXPECT_FALSE(lineAvailable());
}

// Test 3: Queue overflow behavior
TEST_F(PLUARTQueueTest, QueueOverflow) {
  char line_buffer[50];

  // Fill the queue to capacity
  for (int i = 0; i < LINE_QUEUE_DEPTH; i++) {
    snprintf(line_buffer, sizeof(line_buffer), "LINE_%d\n", i);
    EXPECT_TRUE(enqueueLine(line_buffer, strlen(line_buffer)));
    EXPECT_EQ(dropped_lines, 0) << "No lines should be dropped yet";
  }

  // Try to add one more line (should fail and increment dropped counter)
  const char *overflow_line = "OVERFLOW_LINE\n";
  EXPECT_FALSE(enqueueLine(overflow_line, strlen(overflow_line)));
  EXPECT_EQ(dropped_lines, 1) << "One line should be dropped";

  // Try to add another (should increment again)
  EXPECT_FALSE(enqueueLine(overflow_line, strlen(overflow_line)));
  EXPECT_EQ(dropped_lines, 2) << "Two lines should be dropped";

  // Verify queue still has original lines
  char read_buffer[100];
  for (int i = 0; i < LINE_QUEUE_DEPTH; i++) {
    snprintf(line_buffer, sizeof(line_buffer), "LINE_%d\n", i);
    uint16_t read_len = dequeueLine(read_buffer, sizeof(read_buffer));
    EXPECT_EQ(read_len, strlen(line_buffer));
    EXPECT_EQ(memcmp(read_buffer, line_buffer, read_len), 0);
  }

  EXPECT_FALSE(lineAvailable());
}

// Test 4: Queue recovery after overflow
TEST_F(PLUARTQueueTest, QueueRecoveryAfterOverflow) {
  char line_buffer[50];

  // Fill the queue
  for (int i = 0; i < LINE_QUEUE_DEPTH; i++) {
    snprintf(line_buffer, sizeof(line_buffer), "LINE_%d\n", i);
    EXPECT_TRUE(enqueueLine(line_buffer, strlen(line_buffer)));
  }

  // Overflow
  EXPECT_FALSE(enqueueLine("OVERFLOW\n", 9));
  EXPECT_EQ(dropped_lines, 1);

  // Read one line to make space
  char read_buffer[100];
  uint16_t read_len = dequeueLine(read_buffer, sizeof(read_buffer));
  EXPECT_GT(read_len, 0);

  // Now should be able to enqueue again
  const char *new_line = "NEW_LINE\n";
  EXPECT_TRUE(enqueueLine(new_line, strlen(new_line)));
}

// Test 5: Empty queue read returns 0
TEST_F(PLUARTQueueTest, EmptyQueueRead) {
  char read_buffer[100];

  EXPECT_FALSE(lineAvailable());
  uint16_t read_len = dequeueLine(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(read_len, 0) << "Reading from empty queue should return 0";
}

// Test 6: Large line handling
TEST_F(PLUARTQueueTest, LargeLine) {
  char large_line[LPUART1_LINE_BUFF_LEN];
  char read_buffer[LPUART1_LINE_BUFF_LEN];

  // Create a large line (but within buffer limits)
  size_t large_len = LPUART1_LINE_BUFF_LEN - 100;
  memset(large_line, 'A', large_len - 1);
  large_line[large_len - 1] = '\n';

  EXPECT_TRUE(enqueueLine(large_line, large_len));
  EXPECT_TRUE(lineAvailable());

  uint16_t read_len = dequeueLine(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(read_len, large_len);
  EXPECT_EQ(memcmp(read_buffer, large_line, read_len), 0);
}

// Test 7: Buffer size limiting on read
TEST_F(PLUARTQueueTest, ReadBufferSizeLimiting) {
  const char *test_line = "THIS_IS_A_LONG_LINE\n";
  char small_buffer[10];

  EXPECT_TRUE(enqueueLine(test_line, strlen(test_line)));

  // Read with smaller buffer - should be truncated
  uint16_t read_len = dequeueLine(small_buffer, sizeof(small_buffer));
  EXPECT_EQ(read_len, sizeof(small_buffer));
  EXPECT_EQ(memcmp(small_buffer, test_line, sizeof(small_buffer)), 0);
}

// Test 8: Mixed size lines
TEST_F(PLUARTQueueTest, MixedSizeLines) {
  const char *lines[] = {"SHORT\n", "A_MUCH_LONGER_LINE_WITH_MORE_CONTENT\n", "MED\n",
                         "TINY\n"};
  char read_buffer[100];

  for (int i = 0; i < 4; i++) {
    EXPECT_TRUE(enqueueLine(lines[i], strlen(lines[i])));
  }

  for (int i = 0; i < 4; i++) {
    uint16_t read_len = dequeueLine(read_buffer, sizeof(read_buffer));
    EXPECT_EQ(read_len, strlen(lines[i]));
    EXPECT_EQ(memcmp(read_buffer, lines[i], read_len), 0);
  }
}

// Test 9: Queue messages waiting count
TEST_F(PLUARTQueueTest, QueueMessagesWaiting) {
  EXPECT_EQ(uxQueueMessagesWaiting(test_queue), 0);

  // Add lines and check count
  for (int i = 1; i <= 5; i++) {
    char line[20];
    snprintf(line, sizeof(line), "LINE_%d\n", i);
    EXPECT_TRUE(enqueueLine(line, strlen(line)));
    EXPECT_EQ(uxQueueMessagesWaiting(test_queue), i);
  }

  // Remove lines and check count
  char read_buffer[100];
  for (int i = 5; i > 0; i--) {
    dequeueLine(read_buffer, sizeof(read_buffer));
    EXPECT_EQ(uxQueueMessagesWaiting(test_queue), i - 1);
  }
}

// Test 10: Queue reset behavior
TEST_F(PLUARTQueueTest, QueueReset) {
  // Fill queue with data
  for (int i = 0; i < 5; i++) {
    char line[20];
    snprintf(line, sizeof(line), "LINE_%d\n", i);
    EXPECT_TRUE(enqueueLine(line, strlen(line)));
  }

  EXPECT_TRUE(lineAvailable());
  EXPECT_EQ(uxQueueMessagesWaiting(test_queue), 5);

  // Reset queue (simulates flush() behavior)
  xQueueReset(test_queue);

  EXPECT_FALSE(lineAvailable());
  EXPECT_EQ(uxQueueMessagesWaiting(test_queue), 0);

  // Verify queue is usable after reset
  const char *new_line = "AFTER_RESET\n";
  EXPECT_TRUE(enqueueLine(new_line, strlen(new_line)));
  EXPECT_TRUE(lineAvailable());
}

// Test 11: Stress test - rapid enqueue/dequeue
TEST_F(PLUARTQueueTest, RapidEnqueueDequeue) {
  char line_buffer[50];
  char read_buffer[100];

  for (int cycle = 0; cycle < 20; cycle++) {
    // Enqueue some lines
    int lines_to_add = (cycle % (LINE_QUEUE_DEPTH - 1)) + 1;
    for (int i = 0; i < lines_to_add; i++) {
      snprintf(line_buffer, sizeof(line_buffer), "CYCLE_%d_LINE_%d\n", cycle, i);
      EXPECT_TRUE(enqueueLine(line_buffer, strlen(line_buffer)));
    }

    // Dequeue all lines
    for (int i = 0; i < lines_to_add; i++) {
      snprintf(line_buffer, sizeof(line_buffer), "CYCLE_%d_LINE_%d\n", cycle, i);
      uint16_t read_len = dequeueLine(read_buffer, sizeof(read_buffer));
      EXPECT_EQ(read_len, strlen(line_buffer));
      EXPECT_EQ(memcmp(read_buffer, line_buffer, read_len), 0);
    }

    EXPECT_FALSE(lineAvailable());
  }
}
