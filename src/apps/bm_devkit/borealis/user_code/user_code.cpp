#include "bm_borealis.h"
#include "cbor.h"
#include "device_info.h"
#include "pubsub.h"
#include "stm32_rtc.h"
#include "uptime.h"

#define SENSOR_TOPIC_PREFIX "sensor/"
#define PSPL_TOPIC_SUFFIX "/aos/borealis/levels"
#define PSPLS_TOPIC_SUFFIX "/aos/borealis/level_statistics"

const char b64_hdr[][53] = {"ocd6ocd6ocd6ocd6ocd6ocd6ocd6ocd6ocd6ocd6ocd6ocd6ocd6",
                            "ocd6ocd6ocd6ocd6ocd6ocd6ocd67777ocd6ocd6ocd6ocd6ocd6",
                            "E3uqaIqh+zmcYYmQ3hiKaViGOxiC6Dd9jnd4mGd5n0d6oCd6ocd6",
                            "TIuzKDutSwqdj2mUB9mKdZiGOaiB51d8hId3fGd4j2d5obd6rzd8",
                            "UNy+nLuz7Rqpaiqi+BmZHBmNeJiC7Td8hTd3e8d3gyd5ntd6vhd9",
                            "gfuy79qnTAqenkmWJTmMkfiFOgiC42d8iXd3h9d4mMd5pPd6sad7",
                            "d7uz/qqseiqg3Yme4fmQtpiHTBiB3nd7gFd3gpd4kGd5ogd7u6d8",
                            "afuxz9qmGcqflcmYS4mOoGiIURiC6ad7fvd2ifd4mRd6pBd6rFd7",
                            "xUu5YSuzy/qnKTqdWbmQqqiHRmiC6hd8iEd3gWd5lLd5o4d6tId7",
                            "87qoJwqisqmWSymQtoiKe/iGNXiB8Wd9jed3iJd5n/d5nOd5mwd6",
                            "mCu1AsuqkBql5dmaaKmPrziJQuiA6Dd8g+d2jTd4jGd5npd6vwd9",
                            "x1qn+DmaqVmXBFmMuCiJcliGO/iC55d8lad4kwd6oid6l8d5nQd6",
                            "aJuy/HqoSJqemimYhEmLjqiFQ3iB4hd8iSd3iKd5mYd5m2d6tNd7",
                            "dKqf1smWC+mMhRiJeDiGariFPBiB0yd8ipd5kjd5n7d5l6d5lHd5",
                            "w3u2OMuusCqo/omchEmQrqiJSyiC4Nd7gvd47Hd4izd5oLd6uud8",
                            "djuw7RqoVBqj9EmZXDmPqUiHSIiB25d8f7d3p4d4lZd5o8d6tld8",
                            "OeuvxZqnCeqciImavKmQl4iHL3iB3qd8iud2jDd5k2d5n6d6svd7",
                            "kOu2K5usmrqgARqarKmPl1iGOIiA32d8ifd3iyd5lqd5nUd6r3d7"};

const char b64_ldr[][261] = {
    "/ymtVju7tHyahLqveavJYQmkvCqzZNyO9fmm6mrBvhibNKqr7JuJaomecbq8aPiQmumhhVuG+oiW6DmzaRiMN/"
    "ma2vqFi1iMJvmpWViHjtiMPYqEXxiHlQigIniDQpiFf8l/EKiBKOiPz/d96Cd/"
    "oBh7wGd80NeCeWd4iyd5DIh2dfd3hzeAcyd4jFd5PMh3jUd5nGeFgwd5mOd5bch4lrd5n/"
    "d7kbd5nxd6qbd4n2d6qyd7iSd6rpd72wd5qId7yId+",

    "/pmtQvu74+yaf3qvf1vKVPmjtOqya5yQ8Cmm6grEwviaJUqrExyJZ7mdb7q+"
    "YaiQhbmhtcuF7jiW5jm3YOiLMXmbGcuFiZiLK7mqUkiHiSiNTKqEXtiGm+ieIniDRfiFlQmAEZiBKZiU0Rd+6Ed/"
    "BAl7v1d80HeIeVd4iyd5QEh2d/d3hjeDbid4i1d5H+h3jld5nCeMghd5med5ydh4l7d5n/"
    "d8j7d5ngd6sMd4nmd6qud7iyd6rZd76gd5qHd7x4d/",

    "C+qvZ9u88PyZm/qwlcvKUnmlwqq1ePyRB6qnDIvEypicQ7qsD1yJhpmfg/q8epiTp3mjuOuG/"
    "4iX90m3aSiNS1mbD2uGluiMRhmsW4iHlsiNXnqEYDiHpKihJ4iDSOiFs/l/D4iBJgiTzfd95zd/"
    "1Ah7v2d8z+eFdkd4ixd5/"
    "kd2ddd3hEd8cQd4iSd5MPh3jEd5nLeHhfd4lsd5rbh4lZd5nhd9kqd5nQd6qvd4oXd6qxd7i1d6sad7yCd5rdd7zId"
    "9",

    "L6quYtu9AV2bhxqxipvNXomky4q0reyQBsqnEZvF18iaNkqtKfyIZ5meezq/"
    "djiRoymknNuG5aiW+Am1aCiMMXmbFruFikiMS/mpVBiHjpiOT2qEXLiGmjigF+iCO5iFrzl/"
    "BeiAICiUx2d94Md+NGl6ted7yJeKcBd4h/"
    "d4rjh2c8d3hUeGbRd4i2d5QNh3jDd5nEePgvd4ltd52Zh4lad5nheCkqd5nfd53dd4nWd6qpd8jQd6r6d77Sd5prd7"
    "y9d/"};

static uint8_t cbor_buffer[1024];
static uint64_t lastHdrPub = 0;
static uint64_t lastLdrPub = 0;
static bool firstRtc = false;
static char topic_hdr[44];
static char topic_ldr[54];

void setup(void) {
  if (snprintf(topic_hdr, sizeof(topic_hdr),
               SENSOR_TOPIC_PREFIX "%016" PRIx64 PSPL_TOPIC_SUFFIX,
               getNodeId()) >= (ssize_t)sizeof(topic_hdr)) {
    printf("error: %s: could not construct hdr pspl topic string\n", __func__);
  }
  if (snprintf(topic_ldr, sizeof(topic_ldr),
               SENSOR_TOPIC_PREFIX "%016" PRIx64 PSPLS_TOPIC_SUFFIX,
               getNodeId()) >= (ssize_t)sizeof(topic_ldr)) {
    printf("error: %s: could not construct ldr pspls topic string\n", __func__);
  }
}

static void publish_hdr(void) {
  static uint8_t hdr_message_counter = 0;
  struct borealis_levels d = {.header = {.version = 0,
                                         .reading_time_utc_ms = 0,
                                         .reading_uptime_millis = uptimeGetMs(),
                                         .sensor_reading_time_ms = 0},
                              .dt = 0.983f,
                              .first_band_index = 16,
                              .levels = (char *)b64_hdr[hdr_message_counter++ % 18],
                              .levels_length = 52};

  size_t encoded_len = 0;
  CborError c_err;
  if ((c_err = borealis_levels_encode(&d, cbor_buffer, sizeof(cbor_buffer), &encoded_len)) !=
      CborNoError) {
    printf("error: %s: borealis_levels_encode returns %d\r\n", __func__, c_err);
    return;
  }

  BmErr b_err = bm_pub(topic_hdr, cbor_buffer, encoded_len, 0, 1);
  if (b_err != BmOK) {
    printf("%s: %llu: sending hdr pspl cbor message of length %u, bm_pub returns %d\r\n",
           __func__, uptimeGetMs(), (unsigned)encoded_len, b_err);
  }
}

static void publish_ldr(void) {
  static uint8_t ldr_message_counter = 0;
  struct borealis_level_statistics d = {.header = {.version = 0,
                                                   .reading_time_utc_ms = 0,
                                                   .reading_uptime_millis = uptimeGetMs(),
                                                   .sensor_reading_time_ms = 0},
                                        .dt = 0.983f,
                                        .dt_report = 299.8272f,
                                        .first_band_index = 16,
                                        .levels = (char *)b64_ldr[ldr_message_counter++ % 4],
                                        .levels_length = 260,
                                        .max_iqr = 0.1337f};

  size_t encoded_len = 0;
  CborError c_err;
  if ((c_err = borealis_levels_statistics_encode(&d, cbor_buffer, sizeof(cbor_buffer),
                                                 &encoded_len)) != CborNoError) {
    printf("error: %s: borealis_levels_statistics_encode returns %d\r\n", __func__, c_err);
    return;
  }

  BmErr b_err = bm_pub(topic_ldr, cbor_buffer, encoded_len, 0, 1);
  if (b_err != BmOK) {
    printf("%s: %llu: sending ldr pspls cbor message of length %u, bm_pub returns %d\r\n",
           __func__, uptimeGetMs(), (unsigned)encoded_len, b_err);
  }
}

void loop(void) {
  if (!isRTCSet()) {
    return;
  }

  const uint64_t now = uptimeGetMicroSeconds();

  if (!firstRtc) {
    printf("RTC first set\n");
    firstRtc = true;
    lastHdrPub = now;
    lastLdrPub = now;
  }

  if (now - lastHdrPub > 983040U) {
    lastHdrPub = now;
    publish_hdr();
  }
  if (now - lastLdrPub > 299827200U) {
    lastLdrPub = now;
    publish_ldr();
  }
}
