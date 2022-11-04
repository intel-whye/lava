// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause
// See: https://spdx.org/licenses/

#include <message_infrastructure/csrc/channel/shmem/shm.h>

namespace message_infrastructure {

SharedMemory::SharedMemory(const size_t &mem_size,
                           void* mmap,
                           const int &key) {
  data_ = mmap;
  size_ = mem_size;
  req_name_ += std::to_string(key);
  ack_name_ += std::to_string(key);
}

SharedMemory::SharedMemory(const size_t &mem_size, void* mmap) {
  data_ = mmap;
  size_ = mem_size;
}

SharedMemory::~SharedMemory() {
  // Close();
  munmap(data_, size_);
}

void SharedMemory::InitSemaphore(sem_t* req, sem_t * ack) {
  // req_ = sem_open(req_name_.c_str(), O_CREAT, 0644, 0);
  // ack_ = sem_open(ack_name_.c_str(), O_CREAT, 0644, 1);
  // if (req_ != SEM_FAILED) {
  //   LAVA_LOG(LOG_SMMP, "Open sem req_, namd: %s\n", req_name_.c_str());
  // } else {
  //   LAVA_LOG_ERR("Open sem req_ failed, name: %s, errno: %d\n",
  //                req_name_.c_str(), errno);
  // }
  // if (ack_ != SEM_FAILED) {
  //   LAVA_LOG(LOG_SMMP, "Open sem ack_, namd: %s\n", ack_name_.c_str());
  // } else {
  //   LAVA_LOG_ERR("Open sem ack_ failed, name: %s, errno: %d\n",
  //                ack_name_.c_str(), errno);
  // }
  req_ = req;
  ack_ = ack;
}

void SharedMemory::Start() {
}

void SharedMemory::Store(HandleFn store_fn) {
  sem_wait(ack_);
  store_fn(data_);
  sem_post(req_);
}

bool SharedMemory::Load(HandleFn consume_fn) {
  bool ret = false;
  if (!sem_trywait(req_)) {
    LAVA_DEBUG(LOG_SMMP, "shm load post sem_trywait get in\n");
    consume_fn(data_);
    sem_post(ack_);
    ret = true;
  }
  return ret;
}

void SharedMemory::BlockLoad(HandleFn consume_fn) {
  sem_wait(req_);
  consume_fn(data_);
  sem_post(ack_);
}

void SharedMemory::Read(HandleFn consume_fn) {
  sem_wait(req_);
  consume_fn(data_);
  sem_post(req_);
}

bool SharedMemory::TryProbe() {
  int val;
  sem_getvalue(req_, &val);
  return val > 0;
}
  char __size[__SIZEOF_SEM_T];
  long int __align;
void SharedMemory::Close() {
  // LAVA_DUMP(LOG_SMMP, "req_->__size: %p %p %p %p\nreq_->__align: %ld\n", req_->__size[0], req_->__size[1], req_->__size[2], req_->__size[3], req_->__align);
  // LAVA_DUMP(LOG_SMMP, "ack_->__size: %p %p %p %p\nack_->__align: %ld\n", ack_->__size[0], ack_->__size[1], ack_->__size[2], ack_->__size[3], ack_->__align);
  // LAVA_ASSERT_INT(sem_close(req_), 0);
  // LAVA_ASSERT_INT(sem_close(ack_), 0);
}

std::string SharedMemory::GetReq() {
  return req_name_;
}

std::string SharedMemory::GetAck() {
  return ack_name_;
}

int SharedMemory::GetDataElem(int offset) {
  return static_cast<int>(*(reinterpret_cast<char*>(data_) + offset));
}

RwSharedMemory::RwSharedMemory(const size_t &mem_size,
                               void* mmap,
                               const int &key)
  : size_(mem_size), data_(mmap) {
  sem_name_ += std::to_string(key);
}

RwSharedMemory::~RwSharedMemory() {
  Close();
  munmap(data_, size_);
}

void RwSharedMemory::InitSemaphore() {
  sem_ = sem_open(sem_name_.c_str(), O_CREAT, 0644, 0);
}

void RwSharedMemory::Start() {
  sem_post(sem_);
}

void RwSharedMemory::Handle(HandleFn handle_fn) {
  sem_wait(sem_);
  handle_fn(data_);
  sem_post(sem_);
}

void RwSharedMemory::Close() {
  LAVA_ASSERT_INT(sem_close(sem_), 0);
}

void SharedMemManager::DeleteAllSharedMemory() {
  int result = 0;
  LAVA_DEBUG(LOG_SMMP, "Delete: Number of shm to free: %zd.\n",
             shm_fd_strs_.size());
  LAVA_DEBUG(LOG_SMMP, "Delete: Number of sem to free: %zd.\n",
             sem_d_strs_.size());
  for (auto const& it : shm_fd_strs_) {
    LAVA_DEBUG(LOG_SMMP, "Shm fd and name close: %s %d\n",
               it.second.c_str(), it.first);
    LAVA_ASSERT_INT(shm_unlink(it.second.c_str()), 0);
    LAVA_ASSERT_INT(close(it.first), 0);
  }
  // for (auto it = sem_strs_.begin(); it != sem_strs_.end(); it++) {
  //   LAVA_DEBUG(LOG_SMMP, "Sem name unlink: %s\n", it->c_str());
  //   LAVA_ASSERT_INT(sem_unlink(it->c_str()), 0);
  // }
  for (auto const& it : sem_d_strs_) {
    LAVA_DEBUG(LOG_SMMP, "Sem close and unlink: %s\n", it.second.c_str());
    LAVA_ASSERT_INT(sem_unlink(it.second.c_str()), 0);
    LAVA_ASSERT_INT(sem_close(it.first), 0);
  }
  // sem_strs_.clear();
  sem_d_strs_.clear();
  shm_fd_strs_.clear();
}

SharedMemManager SharedMemManager::smm_;

SharedMemManager& GetSharedMemManager() {
  SharedMemManager &smm = SharedMemManager::smm_;
  return smm;
}
}  // namespace message_infrastructure
