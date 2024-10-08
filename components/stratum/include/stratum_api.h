#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"
#include <stdbool.h>
#include <stdint.h>

#define MAX_MERKLE_BRANCHES 32
#define HASH_SIZE 32
#define COINBASE_SIZE 100
#define COINBASE2_SIZE 128

typedef enum
{
    STRATUM_UNKNOWN,
    MINING_NOTIFY,
    MINING_SET_DIFFICULTY,
    MINING_SET_VERSION_MASK,
    STRATUM_RESULT,
    STRATUM_RESULT_SETUP,
    STRATUM_RESULT_VERSION_MASK,
    STRATUM_RESULT_SUBSCRIBE,
    CLIENT_RECONNECT
} stratum_method;

static const int STRATUM_ID_SUBSCRIBE = 1;
static const int STRATUM_ID_CONFIGURE = 2;

typedef struct
{
    char *job_id;
    uint8_t _prev_block_hash[HASH_SIZE];
    char *coinbase_1;
    char *coinbase_2;
    uint8_t _merkle_branches[MAX_MERKLE_BRANCHES][HASH_SIZE];
    size_t n_merkle_branches;
    uint32_t version;
    uint32_t version_mask;
    uint32_t target;
    uint32_t ntime;
    uint32_t difficulty;
} mining_notify;

typedef struct
{
    char *extranonce_str;
    int extranonce_2_len;

    int64_t message_id;
    // Indicates the type of request the message represents.
    stratum_method method;

    // mining.notify
    int should_abandon_work;
    mining_notify *mining_notification;
    // mining.set_difficulty
    uint32_t new_difficulty;
    // mining.set_version_mask
    uint32_t version_mask;
    // result
    bool response_success;
} StratumApiV1Message;

void STRATUM_V1_reset_uid();

void STRATUM_V1_initialize_buffer();

char *STRATUM_V1_receive_jsonrpc_line(int sockfd);

int STRATUM_V1_subscribe(int socket, const char* device, const char* asic);

void STRATUM_V1_parse(StratumApiV1Message *message, const char *stratum_json);

void STRATUM_V1_free_mining_notify(mining_notify *params);

int STRATUM_V1_authenticate(int socket, const char *username, const char *pass);

void STRATUM_V1_configure_version_rolling(int socket);

int STRATUM_V1_suggest_difficulty(int socket, uint32_t difficulty);

void STRATUM_V1_submit_share(int socket, const char *username, const char *jobid, const char *extranonce_2, const uint32_t ntime,
                             const uint32_t nonce, const uint32_t version);

#ifdef __cplusplus
}
#endif
