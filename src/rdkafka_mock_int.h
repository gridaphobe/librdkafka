/*
 * librdkafka - Apache Kafka C library
 *
 * Copyright (c) 2019 Magnus Edenhill
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RDKAFKA_MOCK_INT_H_
#define _RDKAFKA_MOCK_INT_H_

/**
 * @struct A real TCP connection from the client to a mock broker.
 */
typedef struct rd_kafka_mock_connection_s {
        TAILQ_ENTRY(rd_kafka_mock_connection_s) link;
        rd_kafka_transport_t *transport; /**< Socket transport */
        rd_kafka_buf_t *rxbuf;   /**< Receive buffer */
        rd_kafka_bufq_t outbufs; /**< Send buffers */
        short *poll_events;      /**< Events to poll, points to
                                  *   the broker's pfd array */
        struct sockaddr_in peer; /**< Peer address */
        struct rd_kafka_mock_broker_s *broker;
} rd_kafka_mock_connection_t;


/**
 * @struct Mock broker
 */
typedef struct rd_kafka_mock_broker_s {
        TAILQ_ENTRY(rd_kafka_mock_broker_s) link;
        int32_t id;
        char    advertised_listener[128];
        int     port;
        char   *rack;

        int     listen_s;   /**< listen() socket */

        TAILQ_HEAD(, rd_kafka_mock_connection_s) connections;

        struct rd_kafka_mock_cluster_s *cluster;
} rd_kafka_mock_broker_t;


/**
 * @struct Mock partition
 */
typedef struct rd_kafka_mock_partition_s {
        TAILQ_ENTRY(rd_kafka_mock_partition_s) leader_link;
        int32_t id;

        int64_t start_offset;
        int64_t last_offset;

        rd_kafka_mock_broker_t  *leader;
        rd_kafka_mock_broker_t **replicas;
        int                      replica_cnt;

        struct rd_kafka_mock_topic_s *topic;
} rd_kafka_mock_partition_t;


/**
 * @struct Mock topic
 */
typedef struct rd_kafka_mock_topic_s {
        TAILQ_ENTRY(rd_kafka_mock_topic_s) link;
        char   *name;

        rd_kafka_mock_partition_t *partitions;
        int     partition_cnt;

        struct rd_kafka_mock_cluster_s *cluster;
} rd_kafka_mock_topic_t;


typedef void (rd_kafka_mock_io_handler_t) (struct rd_kafka_mock_cluster_s
                                           *mcluster,
                                           int fd, int events, void *opaque);

/**
 * @struct Mock cluster.
 *
 * The cluster IO loop runs in a separate thread where all
 * broker IO is handled.
 *
 * No locking is needed.
 */
typedef struct rd_kafka_mock_cluster_s {
        char id[32];  /**< Generated cluster id */

        rd_kafka_t *rk;

        int32_t controller_id;  /**< Current controller */

        TAILQ_HEAD(, rd_kafka_mock_broker_s) brokers;
        int broker_cnt;

        TAILQ_HEAD(, rd_kafka_mock_topic_s) topics;
        int topic_cnt;

        char *bootstraps; /**< bootstrap.servers */

        thrd_t thread;    /**< Mock thread */

        rd_kafka_q_t *ops; /**< Control ops queue for interacting with the
                            *   cluster. */

        int wakeup_fds[2];  /**< Wake-up fds for use with .ops */

        rd_bool_t run;    /**< Cluster will run while this value is true */

        int                         fd_cnt;   /**< Number of file descriptors */
        int                         fd_size;  /**< Allocated size of .fds
                                               *   and .handlers */
        struct pollfd              *fds;      /**< Dynamic array */

        rd_kafka_broker_t *dummy_rkb;  /**< Some internal librdkafka APIs
                                        *   that we are reusing requires a
                                        *   broker object, we use the
                                        *   internal broker and store it
                                        *   here for convenient access. */

        struct {
                int partition_cnt;      /**< Auto topic create part cnt */
                int replication_factor; /**< Auto topic create repl factor */
        } defaults;

        /**< Dynamic array of IO handlers for corresponding fd in .fds */
        struct {
                rd_kafka_mock_io_handler_t *cb; /**< Callback */
                void *opaque;                   /**< Callbacks' opaque */
        } *handlers;
} rd_kafka_mock_cluster_t;



struct rd_kafka_mock_api_handler {
        int16_t MinVersion;
        int16_t MaxVersion;
        int (*cb) (rd_kafka_mock_connection_t *mconn, rd_kafka_buf_t *rkbuf);
};

extern const struct rd_kafka_mock_api_handler
rd_kafka_mock_api_handlers[RD_KAFKAP__NUM];



rd_kafka_buf_t *rd_kafka_mock_buf_new_response (const rd_kafka_buf_t *request);
void rd_kafka_mock_connection_send_response (rd_kafka_mock_connection_t *mconn,
                                             const rd_kafka_buf_t *request,
                                             rd_kafka_buf_t *resp);

rd_kafka_mock_partition_t *
rd_kafka_mock_partition_find (const rd_kafka_mock_topic_t *mtopic,
                              int32_t partition);
rd_kafka_mock_topic_t *
rd_kafka_mock_topic_auto_create (rd_kafka_mock_cluster_t *mcluster,
                                 const char *topic,
                                 rd_kafka_resp_err_t *errp);
rd_kafka_mock_topic_t *
rd_kafka_mock_topic_find (const rd_kafka_mock_cluster_t *mcluster,
                          const char *name);
rd_kafka_mock_topic_t *
rd_kafka_mock_topic_find_by_kstr (const rd_kafka_mock_cluster_t *mcluster,
                                  const rd_kafkap_str_t *kname);
rd_kafka_mock_broker_t *
rd_kafka_mock_cluster_get_coord (rd_kafka_mock_cluster_t *mcluster,
                                 rd_kafka_coordtype_t KeyType,
                                 const rd_kafkap_str_t *Key);

#include "rdkafka_mock.h"

#endif /* _RDKAFKA_MOCK_INT_H_ */
