// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause
// See: https://spdx.org/licenses/

#include <message_infrastructure_logging.h>

namespace message_infrastructure {

LogMsg::LogMsg(const char &msg_data){
    this->msg_data_ = msg_data;
    this->msg_time_ = getTime();
}

LogMsg::~LogMsg(){

}

} // namespace message_infrastructure