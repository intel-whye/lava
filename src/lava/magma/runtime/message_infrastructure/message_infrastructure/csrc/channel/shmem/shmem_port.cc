// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause
// See: https://spdx.org/licenses/

#include <message_infrastructure/csrc/channel/shmem/shmem_port.h>
#include <message_infrastructure/csrc/core/utils.h>
#include <message_infrastructure/csrc/core/message_infrastructure_logging.h>
#include <semaphore.h>
#include <unistd.h>
#include <thread>  // NOLINT
#include <mutex>  // NOLINT
#include <memory>
#include <string>
#include <condition_variable>  // NOLINT
#include <cassert>
#include <cstring>
#include <sys/mman.h>

namespace message_infrastructure {

template<>
void RecvQueue<MetaDataPtr>::FreeData(MetaDataPtr data) {
  free(data->mdata);
}

ShmemSendPort::ShmemSendPort(const std::string &name,
                SharedMemoryPtr shm,
                const size_t &size,
                const size_t &nbytes)
  : AbstractSendPort(name, size, nbytes), shm_(shm), done_(false)
{}

void ShmemSendPort::Start() {
  shm_->Start();
}

void ShmemSendPort::Send(MetaDataPtr metadata) {
  shm_->Store([this, &metadata](void* data){
    char* cptr = reinterpret_cast<char*>(data);
    std::memcpy(cptr, metadata.get(), sizeof(MetaData));
    cptr += sizeof(MetaData);
    std::memcpy(cptr, metadata->mdata, this->nbytes_ - sizeof(MetaData));
  });
}

bool ShmemSendPort::Probe() {
  return false;
}

void ShmemSendPort::Join() {
  done_ = true;
}

ShmemRecvPort::ShmemRecvPort(const std::string &name,
                SharedMemoryPtr shm,
                const size_t &size,
                const size_t &nbytes)
  : AbstractRecvPort(name, size, nbytes), shm_(shm), done_(false) {
  recv_queue_ = std::make_shared<RecvQueue<MetaDataPtr>>(name_, size_);
}

ShmemRecvPort::~ShmemRecvPort() {
}

void ShmemRecvPort::Start() {
  recv_queue_thread_ = std::make_shared<std::thread>(
                       &message_infrastructure::ShmemRecvPort::QueueRecv, this);
}

void ShmemRecvPort::QueueRecv() {
  while (!done_.load()) {
    bool ret = false;
    if (this->recv_queue_->AvailableCount() > 0) {
      ret = shm_->Load([this](void* data){
        MetaDataPtr metadata_res = std::make_shared<MetaData>();
        std::memcpy(metadata_res.get(), data, sizeof(MetaData));
        int mem_size = (this->nbytes_ - sizeof(MetaData) + 7) & (~0x7);
        // if (mem_size+16 <= getpagesize()/256) {
          LAVA_LOG(LOG_SMMP, "memory allocates, size: %d\n", mem_size+16);
          metadata_res->mdata = calloc(mem_size, 1);
        // } else {
        //   LAVA_LOG(LOG_SMMP, "memory maps, size: %d\n", mem_size+16);
        //   int fd = open("/dev/zero", O_RDWR);
        //   metadata_res->mdata = mmap(0,(16 + mem_size), PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
        //   close(fd);
        //   *((uint64_t*)metadata_res->mdata+1) = 16 + mem_size;
        //   metadata_res->mdata = (char*)metadata_res->mdata + 16;
        // }
        if (metadata_res->mdata == NULL) {
          LAVA_LOG_ERR("errno: %d\n", errno);
          LAVA_ASSERT_INT(-1, 0);
        }
        std::memcpy(metadata_res->mdata,
          reinterpret_cast<char *>(data) + sizeof(MetaData),
          this->nbytes_ - sizeof(MetaData));
        this->recv_queue_->Push(metadata_res);
      });
    }
    if (!ret) {
      // sleep
      helper::Sleep();
    }
  }
}

bool ShmemRecvPort::Probe() {
  return recv_queue_->Probe();
}

MetaDataPtr ShmemRecvPort::Recv() {
  return recv_queue_->Pop(true);
}

void ShmemRecvPort::Join() {
  if (!done_) {
    done_ = true;
    recv_queue_thread_->join();
    recv_queue_->Stop();
    shm_->Close();
  }
}

MetaDataPtr ShmemRecvPort::Peek() {
  return recv_queue_->Front();
}

ShmemBlockRecvPort::ShmemBlockRecvPort(const std::string &name,
  SharedMemoryPtr shm, const size_t &nbytes)
  : AbstractRecvPort(name, 1, nbytes), shm_(shm)
{}

MetaDataPtr ShmemBlockRecvPort::Recv() {
  MetaDataPtr metadata_res = std::make_shared<MetaData>();
  shm_->BlockLoad([&metadata_res, this](void* data){
    std::memcpy(metadata_res.get(), data, sizeof(MetaData));
    void *ptr = malloc(this->nbytes_ - sizeof(MetaData));
    std::memcpy(ptr, reinterpret_cast<char *>(data) + sizeof(MetaData),
      this->nbytes_ - sizeof(MetaData));
    metadata_res->mdata = ptr;
  });
  return metadata_res;
}

MetaDataPtr ShmemBlockRecvPort::Peek() {
  MetaDataPtr metadata_res = std::make_shared<MetaData>();
  shm_->Read([&metadata_res, this](void* data){
    std::memcpy(metadata_res.get(), data, sizeof(MetaData));
    void *ptr = malloc(this->nbytes_ - sizeof(MetaData));
    std::memcpy(ptr, reinterpret_cast<char *>(data) + sizeof(MetaData),
      this->nbytes_ - sizeof(MetaData));
    metadata_res->mdata = ptr;
  });
  return metadata_res;
}

bool ShmemBlockRecvPort::Probe() {
  return shm_->TryProbe();
}

}  // namespace message_infrastructure
