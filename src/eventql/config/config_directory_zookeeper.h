/**
 * Copyright (c) 2016 DeepCortex GmbH <legal@eventql.io>
 * Authors:
 *   - Paul Asmuth <paul@eventql.io>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License ("the license") as
 * published by the Free Software Foundation, either version 3 of the License,
 * or any later version.
 *
 * In accordance with Section 7(e) of the license, the licensing of the Program
 * under the license does not imply a trademark license. Therefore any rights,
 * title and interest in our trademarks remain entirely with us.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the license for more details.
 *
 * You can be released from the requirements of the license by purchasing a
 * commercial license. Buying such a license is mandatory as soon as you develop
 * commercial activities involving this program without disclosing the source
 * code of your own applications
 */
#pragma once
#include "eventql/eventql.h"
#include "eventql/config/config_directory.h"
#include "eventql/util/protobuf/msg.h"
#include "eventql/util/thread/threadpool.h"

typedef struct _zhandle zhandle_t;

namespace eventql {

class ZookeeperConfigDirectory : public ConfigDirectory {
public:

  ZookeeperConfigDirectory(
      const String& zookeeper_addrs,
      const String& cluster_name,
      Option<String> server_name,
      String listen_addr);

  ~ZookeeperConfigDirectory();

  String getServerID() const override;

  bool hasServerID() const override;

  virtual bool electLeader() override;

  virtual String getLeader() const override;

  ClusterConfig getClusterConfig() const override;

  void updateClusterConfig(ClusterConfig config) override;

  void setClusterConfigChangeCallback(
      Function<void (const ClusterConfig& cfg)> fn) override;

  ServerConfig getServerConfig(const String& sever_name) const override;

  void updateServerConfig(ServerConfig config) override;

  ReturnCode publishServerStats(ServerStats stats) override;

  Vector<ServerConfig> listServers() const override;

  void setServerConfigChangeCallback(
      Function<void (const ServerConfig& cfg)> fn) override;

  RefPtr<NamespaceConfigRef> getNamespaceConfig(
      const String& customer_key) const override;

  void updateNamespaceConfig(NamespaceConfig config) override;

  void listNamespaces(
      Function<void (const NamespaceConfig& cfg)> fn) const override;

  void setNamespaceConfigChangeCallback(
      Function<void (const NamespaceConfig& cfg)> fn) override;

  TableDefinition getTableConfig(
      const String& db_namespace,
      const String& table_name,
      bool allow_cache = true) override;

  void updateTableConfig(
      const TableDefinition& table,
      bool force = false) override;

  void listTables(
      Function<void (const TableDefinition& tbl)> fn) const override;

  void setTableConfigChangeCallback(
      Function<void (const TableDefinition& tbl)> fn) override;

  Status start(bool create = false) override;
  void stop() override;

  /** don't call this! (can't be private b/c it needs to be called from c binding) */
  void handleZookeeperWatch(int type, int state, String path);

protected:

  enum class ZKState {
    INIT = 0,
    CONNECTING = 1,
    LOADING = 2,
    CONNECTED = 3,
    CONNECTION_LOST = 4,
    CLOSED = 5
  };

  void reconnect(std::unique_lock<std::mutex>* lk);
  Status connect(std::unique_lock<std::mutex>* lk);
  Status load(bool run_callbacks);

  void updateClusterConfigWithLock(ClusterConfig config);

  void handleSessionEvent(int state);
  void handleConnectionEstablished();
  void handleConnectionLost();
  Status handleChangeEvent(const String& vpath, bool run_callbacks);

  Status sync(bool run_callbacks);
  Status syncClusterConfig(bool run_callbacks);
  Status syncLiveServers(bool run_callbacks);
  Status syncLiveServer(bool run_callbacks, const String& server);
  Status syncServers(bool run_callbacks);
  Status syncServer(bool run_callbacks, const String& server);
  Status syncNamespaces(bool run_callbacks);
  Status syncNamespace(bool run_callbacks, const String& ns);
  Status syncTables(bool run_callbacks, const String& ns);
  Status syncTable(
      bool run_callbacks,
      const String& ns,
      const String& table_name);

  bool hasNode(String key);

  bool getNode(
      String key,
      Buffer* buf,
      bool watch = false,
      struct Stat* stat = nullptr) const;

  template <typename T>
  bool getProtoNode(
      String key,
      T* proto,
      bool watch = false,
      struct Stat* stat = nullptr);

  Status listChildren(
      String key,
      Vector<String>* children,
      bool watch = false);

  void runWatchdog();
  void runLogtail();

  const char* getErrorString(int code) const;

  String zookeeper_addrs_;
  size_t zookeeper_timeout_;
  String cluster_name_;
  Option<String> server_name_;
  size_t server_stats_version_;
  String listen_addr_;
  String global_prefix_;
  String path_prefix_;
  ZKState state_;
  zhandle_t* zk_;
  bool is_leader_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  int logpipe_[2];
  FILE* logfile_;

  ClusterConfig cluster_config_;
  HashMap<String, ServerConfig> servers_;
  HashMap<String, ServerStats> servers_live_;
  HashMap<String, NamespaceConfig> namespaces_;
  HashMap<String, TableDefinition> tables_;

  Vector<Function<void (const ClusterConfig& cfg)>> on_cluster_change_;
  Vector<Function<void (const ServerConfig& cfg)>> on_server_change_;
  Vector<Function<void (const NamespaceConfig& cfg)>> on_namespace_change_;
  Vector<Function<void (const TableDefinition& cfg)>> on_table_change_;

  std::thread watchdog_;
  std::thread logtail_;
  thread::ThreadPool callback_scheduler_;
};

template <typename T>
bool ZookeeperConfigDirectory::getProtoNode(
    String key,
    T* proto,
    bool watch /* = false */,
    struct Stat* stat /* = nullptr */) {
  Buffer buf(1024 * 512);
  if (getNode(key, &buf, watch, stat)) {
    msg::decode(buf, proto);
    return true;
  } else {
    return false;
  }
}

} // namespace eventql
