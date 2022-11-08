// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause
// See: https://spdx.org/licenses/

#define NUMPY_CORE_INCLUDE_NUMPY_NPY_1_7_DEPRECATED_API_H_
// to solve the warning "Using deprecated NumPy API,
// disable it with " "#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION"

#include <numpy/arrayobject.h>
#include <Python.h>
#include <message_infrastructure/csrc/port_proxy.h>
#include <message_infrastructure/csrc/core/message_infrastructure_logging.h>
#include <sys/mman.h>

namespace message_infrastructure {

namespace py = pybind11;

std::map<void *, uint64_t> mem_refcount_;
std::mutex mem_refcount_lock_;

void MetaDataDump(MetaDataPtr metadata) {
  int64_t *dims = metadata->dims;
  int64_t *strides = metadata->strides;
  LAVA_DUMP(LOG_LAYER, "MetaData Info:\n"
               "(nd, type, elsize): (%ld, %ld, %ld)\n"
               "total_size: %ld\n"
               "dims:[%ld, %ld, %ld, %ld, %ld]\n"
               "strides:[%ld, %ld, %ld, %ld, %ld]\n",
               metadata->nd,
               metadata->type,
               metadata->elsize,
               metadata->total_size,
               dims[0], dims[1], dims[2], dims[3], dims[4],
               strides[0], strides[1], strides[2], strides[3], strides[4]);
}

ChannelType SendPortProxy::GetChannelType() {
  return channel_type_;
}
void SendPortProxy::Start() {
  send_port_->Start();
}
bool SendPortProxy::Probe() {
  return send_port_->Probe();
}
void SendPortProxy::Send(py::object* object) {
  MetaDataPtr metadata = MDataFromObject_(object);
  send_port_->Send(metadata);
}
void SendPortProxy::Join() {
  send_port_->Join();
}
std::string SendPortProxy::Name() {
  return send_port_->Name();
}
size_t SendPortProxy::Size() {
  return send_port_->Size();
}

ChannelType RecvPortProxy::GetChannelType() {
  return channel_type_;
}
void RecvPortProxy::Start() {
  recv_port_->Start();
}
bool RecvPortProxy::Probe() {
  return recv_port_->Probe();
}
py::object RecvPortProxy::Recv() {
  MetaDataPtr metadata = recv_port_->Recv();
  return MDataToObject_(metadata);
}
void RecvPortProxy::Join() {
  recv_port_->Join();
}
py::object RecvPortProxy::Peek() {
  MetaDataPtr metadata = recv_port_->Peek();
  if (channel_type_ == SHMEMCHANNEL) {
    LAVA_LOG(LOG_LAYER, "Shmem peeks\n");
    return MDataToObject_(metadata, false);
  }
  return MDataToObject_(metadata);
}
std::string RecvPortProxy::Name() {
  return recv_port_->Name();
}
size_t RecvPortProxy::Size() {
  return recv_port_->Size();
}

int trick() {
    // to solve the warning "converting to non-pointer type 'int'
    // from NULL [-Wconversion-null] import_array()"
    _import_array();
    return 0;
}

const int tricky_var = trick();

MetaDataPtr SendPortProxy::MDataFromObject_(py::object* object) {
  PyObject *obj = object->ptr();
  LAVA_LOG(LOG_LAYER, "start MDataFromObject\n");
  if (!PyArray_Check(obj)) {
    LAVA_LOG_ERR("The Object is not array tp is %s\n", Py_TYPE(obj)->tp_name);
    exit(-1);
  }

  LAVA_LOG(LOG_LAYER, "check obj achieved\n");

  auto array = reinterpret_cast<PyArrayObject*> (obj);
  if (!PyArray_ISWRITEABLE(array)) {
    LAVA_LOG(LOG_LAYER, "The array is not writeable\n");
  }

  // var from numpy
  int32_t ndim = PyArray_NDIM(array);
  auto dims = PyArray_DIMS(array);
  auto strides = PyArray_STRIDES(array);
  void* data_ptr = PyArray_DATA(array);
  // auto dtype = PyArray_Type(array);  // no work
  auto dtype = array->descr->type_num;
  auto element_size_in_bytes = PyArray_ITEMSIZE(array);
  auto tsize = PyArray_SIZE(array);

  // set metadata
  MetaDataPtr metadata = std::make_shared<MetaData>();
  metadata->nd = ndim;
  for (int i = 0; i < ndim; i++) {
    metadata->dims[i] = dims[i];
    metadata->strides[i] = strides[i]/element_size_in_bytes;
    if (strides[i] % element_size_in_bytes != 0) {
      LAVA_LOG_ERR("numpy array stride not a multiple of element bytes\n");
    }
  }
  metadata->type = dtype;
  metadata->mdata = data_ptr;
  metadata->elsize = element_size_in_bytes;
  metadata->total_size = tsize;

  return metadata;
}

py::object RecvPortProxy::MDataToObject_(MetaDataPtr metadata, bool cleaner) {
  if (metadata == NULL)
    return py::cast(0);

  std::vector<npy_intp> dims(metadata->nd);
  std::vector<npy_intp> strides(metadata->nd);

  for (int i = 0; i < metadata->nd; i++) {
    dims[i] = metadata->dims[i];
    strides[i] = metadata->strides[i] * metadata->elsize;
  }

  PyObject *array = PyArray_New(
    &PyArray_Type,
    metadata->nd,
    dims.data(),
    metadata->type,
    strides.data(),
    metadata->mdata,
    metadata->elsize,
    NPY_ARRAY_ALIGNED | NPY_ARRAY_WRITEABLE,
    nullptr);

  if (!array)
    return py::cast(0);

  mem_refcount_lock_.lock();
  uint64_t refcount = mem_refcount_[metadata->mdata];
  mem_refcount_lock_.unlock();
  refcount = (refcount + 1);
  if (cleaner) {
    refcount = (refcount | (MEM_REF_FREE_ENABLE));
    // LAVA_DEBUG(LOG_LAYER, "Set memory clean, memory: %p\n", metadata->mdata);
    // capsule = PyCapsule_New(metadata->mdata, NULL,
    //                                   [](PyObject *capsule){
    //   void *memory = PyCapsule_GetPointer(capsule, NULL);
    //   LAVA_DEBUG(LOG_LAYER, "PyObject cleaned, free memory: %p.\n", memory);
    //   // free(memory);
    // });
  } else {
    // capsule = PyCapsule_New(metadata->mdata, NULL,
    //                                   [](PyObject *capsule){
    //   LAVA_DEBUG(LOG_LAYER, "PyObject cleaned, without freeing.\n");
    // });
  }
  mem_refcount_lock_.lock();
  mem_refcount_[metadata->mdata] = refcount;
  mem_refcount_lock_.unlock();
  LAVA_DEBUG(LOG_LAYER, "ref add, memory: %p, refcount: %lx\n", metadata->mdata, refcount);
  PyObject *capsule = PyCapsule_New(metadata->mdata, NULL,
                                      [](PyObject *capsule){
      void *memory = PyCapsule_GetPointer(capsule, NULL);
      mem_refcount_lock_.lock();
      uint64_t refcount = mem_refcount_[memory];
      mem_refcount_lock_.unlock();
      if (refcount & (~MEM_REF_FREE_ENABLE) == 0) {
        LAVA_ASSERT_INT(-1, 0);
      }
      refcount = refcount - 1;
      if (refcount == (MEM_REF_FREE_ENABLE)) {
        LAVA_DEBUG(LOG_LAYER, "PyObject cleaned, free memory: %p, size: %lu.\n", memory, *((uint64_t*)memory - 1)&(~0x7));
        // if ((*((uint64_t*)memory - 1) & (~(uint64_t)0x7)) <= getpagesize()/256){
          free(memory);
        // } else {
        //   munmap((char*)memory-16, *((uint64_t*)memory - 1));
        // }
        mem_refcount_lock_.lock();
        mem_refcount_.erase(memory);
        mem_refcount_lock_.unlock();
      } else if (refcount == 0) {
        mem_refcount_lock_.lock();
        mem_refcount_.erase(memory);
        mem_refcount_lock_.unlock();
        LAVA_DEBUG(LOG_LAYER, "PyObject cleaned, unfree memory: %p.\n", memory);
      } else {
        mem_refcount_lock_.lock();
        mem_refcount_[memory] = refcount;
        mem_refcount_lock_.unlock();
        LAVA_DEBUG(LOG_LAYER, "ref sub, memory: %p, refcount: %lx\n", memory, refcount);
      }
    });
  // Py_INCREF(array);
  LAVA_ASSERT_INT(PyArray_SetBaseObject(reinterpret_cast<PyArrayObject *>(array), capsule), 0);
  return py::reinterpret_steal<py::object>(array);
}

}  // namespace message_infrastructure
