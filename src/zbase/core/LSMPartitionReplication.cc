/**
 * This file is part of the "tsdb" project
 *   Copyright (c) 2015 Paul Asmuth, FnordCorp B.V.
 *
 * FnordMetric is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <zbase/core/LSMPartitionReplication.h>
#include <zbase/core/LSMPartitionReader.h>
#include <zbase/core/ReplicationScheme.h>
#include <stx/logging.h>
#include <stx/io/fileutil.h>
#include <stx/protobuf/msg.h>

using namespace stx;

namespace zbase {

const size_t LSMPartitionReplication::kMaxBatchSizeRows = 8192;
const size_t LSMPartitionReplication::kMaxBatchSizeBytes = 1024 * 1024 * 50; // 50 MB

LSMPartitionReplication::LSMPartitionReplication(
    RefPtr<Partition> partition,
    RefPtr<ReplicationScheme> repl_scheme,
    http::HTTPConnectionPool* http) :
    PartitionReplication(partition, repl_scheme, http) {}

bool LSMPartitionReplication::needsReplication() const {
  auto replicas = repl_scheme_->replicasFor(snap_->key);
  if (replicas.size() == 0) {
    return false;
  }

  auto repl_state = fetchReplicationState();
  auto head_offset = snap_->state.lsm_sequence();
  for (const auto& r : replicas) {
    if (r.is_local) {
      continue;
    }

    const auto& replica_offset = replicatedOffsetFor(repl_state, r.unique_id);
    if (replica_offset < head_offset) {
      return true;
    }
  }

  return false;
}

size_t LSMPartitionReplication::numFullRemoteCopies() const {
  size_t ncopies = 0;
  auto replicas = repl_scheme_->replicasFor(snap_->key);
  auto repl_state = fetchReplicationState();
  auto head_offset = snap_->state.lsm_sequence();

  for (const auto& r : replicas) {
    if (r.is_local) {
      continue;
    }

    const auto& replica_offset = replicatedOffsetFor(repl_state, r.unique_id);
    if (replica_offset >= head_offset) {
      ncopies += 1;
    }
  }

  return ncopies;
}

void LSMPartitionReplication::replicateTo(
    const ReplicaRef& replica,
    uint64_t replicated_offset) {
  if (replica.is_local) {
    RAISE(kIllegalStateError, "can't replicate to myself");
  }

  size_t batch_size = 0;
  size_t num_replicated = 0;
  RecordEnvelopeList batch;
  fetchRecords(
      replicated_offset,
      [this, &batch, &replica, &replicated_offset, &batch_size, &num_replicated] (
          const SHA1Hash& record_id,
          const uint64_t record_version,
          const void* record_data,
          size_t record_size) {
    auto rec = batch.add_records();
    rec->set_tsdb_namespace(snap_->state.tsdb_namespace());
    rec->set_table_name(snap_->state.table_key());
    rec->set_partition_sha1(snap_->key.toString());
    rec->set_record_id(record_id.toString());
    rec->set_record_version(record_version);
    rec->set_record_data(record_data, record_size);

    batch_size += record_size;
    ++num_replicated;

    if (batch_size > kMaxBatchSizeBytes ||
        batch.records().size() > kMaxBatchSizeRows) {
      uploadBatchTo(replica, batch);
      batch.mutable_records()->Clear();
      batch_size = 0;
    }
  });

  if (batch.records().size() > 0) {
    uploadBatchTo(replica, batch);
  }
}

bool LSMPartitionReplication::replicate() {
  iputs("replicate!!", 1);
  auto replicas = repl_scheme_->replicasFor(snap_->key);
  if (replicas.size() == 0) {
    return true;
  }

  auto repl_state = fetchReplicationState();
  auto head_offset = snap_->state.lsm_sequence();
  bool dirty = false;
  bool success = true;

  for (const auto& r : replicas) {
    if (r.is_local) {
      continue;
    }

    const auto& replica_offset = replicatedOffsetFor(repl_state, r.unique_id);

    if (replica_offset < head_offset) {
      logDebug(
          "z1.replication",
          "Replicating partition $0/$1/$2 to $3 ($4 records)",
          snap_->state.tsdb_namespace(),
          snap_->state.table_key(),
          snap_->key.toString(),
          r.addr.hostAndPort(),
          head_offset - replica_offset);

      try {
        replicateTo(r, replica_offset);
        setReplicatedOffsetFor(&repl_state, r.unique_id, head_offset);
        dirty = true;
      } catch (const std::exception& e) {
        success = false;

        stx::logError(
          "z1.replication",
          e,
          "Error while replicating partition $0/$1/$2 to $3",
          snap_->state.tsdb_namespace(),
          snap_->state.table_key(),
          snap_->key.toString(),
          r.addr.hostAndPort());
      }
    }
  }

  if (dirty) {
    commitReplicationState(repl_state);
  }

  return success;
}

void LSMPartitionReplication::uploadBatchTo(
    const ReplicaRef& replica,
    const RecordEnvelopeList& batch) {
  auto body = msg::encode(batch);
  URI uri(StringUtil::format("http://$0/tsdb/replicate", replica.addr.hostAndPort()));
  http::HTTPRequest req(http::HTTPMessage::M_POST, uri.pathAndQuery());
  req.addHeader("Host", uri.hostAndPort());
  req.addHeader("Content-Type", "application/fnord-msg");
  req.addBody(body->data(), body->size());

  auto res = http_->executeRequest(req);
  res.wait();

  const auto& r = res.get();
  if (r.statusCode() != 201) {
    RAISEF(kRuntimeError, "received non-201 response: $0", r.body().toString());
  }

  // FIXME remote commit!
}

} // namespace tdsb

