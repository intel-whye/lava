// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: BSD-3-Clause
// See: https://spdx.org/licenses/

#ifndef MESSAGE_INFRASTRUCTURE_LOGGING_H_
#define MESSAGE_INFRASTRUCTURE_LOGGING_H_

#include<memory>

#include <stdio.h>
#include <memory.h>
#include <time.h>
#define LOG_MP (0)  // log for multiprocessing
#define LOG_ACTOR (0)
#define LOG_LAYER (0)
#define DEBUG_MODE (0)
#define LOG_SMMP (0)  // log for shmemport

#define DEBUG_LOG_TO_FILE (1)
#define DEBUG_LOG_FILE "lava_message_infrastructure.log"

#define MAX_SIZE_PER_LOG_MSG (1024)
#define MAX_SIZE_LOG (16)
#define MAX_SIZE_LOG_TIME (64)

#define LOG_MSG_SUBSTITUTION "!This message was displayed due to the failure of the malloc of this log message!\n"
#define LOG_GET_TIME_FAIL "Get time failed."

#if DEBUG_LOG_TO_FILE
// #define DEBUG_LOG_PRINT(...) do {\
//   int length = 0;\
//   char * log_data = (char*)malloc(sizeof(char)*MAX_SIZE_PER_LOG_MSG);\
//   struct logMsg * log_msg = (struct logMsg*)malloc(sizeof(struct logMsg));\
//   char * log_time = getTime();\
//   if (log_data != NULL)\
//     length = std::snprintf(log_data, MAX_SIZE_PER_LOG_MSG, ## __VA__ARGS__);\
//   if (log_data == NULL || log_msg == NULL || log_time == NULL || length < 0){\
//     if (log_time != NULL){\
//       std::printf(buf);\
//       std::printf(" ");\
//     } \
//     std::printf(## __VA_ARGS__);\
//     free(log_time);\
//     free(log_msg);\
//     free(log_data);\
//   }\
//   else{\
//     log_msg.msg_data = log_data;\
//     log_msg.msg_time = log_time;\
//     getLogInstance().log(log_msg);\
//   }\
// }while(0)
#define DEBUG_LOG_PRINT(_level,_module,  ...) do {\
  int length = 0;\
  char * log_data = (char*)malloc(sizeof(char)*MAX_SIZE_PER_LOG_MSG);\
  if (log_data != NULL)\
    length = std::snprintf(log_data, MAX_SIZE_PER_LOG_MSG, ## __VA__ARGS__);\
  if (log_data == NULL || length <0){\
    getLogInstance().log(LogMsg(std::string(LOG_MSG_SUBSTITUTION)), __FILE__, __LINE__, _module);\
  }else{\
    getLogInstance().log(LogMsg(std::string(log_data)), __FILE__, __LINE__, _module);\
  }\
  free(log_data);\
}while(0)

#else
#define DEBUG_LOG_PRINT(...) std::printf(## __VA_ARGS__)
#endif




#define LAVA_LOG(_cond, _fmt, ...) { \
  if ((_cond)) { \
    std::printf("[CPP INFO]" _fmt, ## __VA_ARGS__); \
  } \
}

#define LAVA_DUMP(_cond, _fmt, ...) { \
  if ((_cond && DEBUG_MODE)) { \
    std::printf(_fmt, ## __VA_ARGS__); \
  } \
}

#define LAVA_DEBUG(_cond, _fmt, ...) { \
  if ((_cond && DEBUG_MODE)) { \
    std::printf("[CPP DEBUG]" _fmt, ## __VA_ARGS__); \
  } \
}

#define LAVA_LOG_WARN(_cond, _fmt, ...) { \
  if ((_cond)) { \
    std::printf("[CPP WARNING] %s " _fmt, __FUNCTION__, ## __VA_ARGS__); \
  } \
}

#define LAVA_LOG_ERR(_fmt, ...) { \
  std::printf("[CPP ERROR] %s " _fmt, __FUNCTION__, ## __VA_ARGS__); \
}

namespace message_infrastructure {

// struct LogMsg{
//   char * msg_time;
//   char * msg_data;
//   char * msg_level;
//   char * msg_module;
// };

class LogMsg{
  public:
    LogMsg(const char&);
    ~LogMsg();
    std::string getEntireLogMsg();

  private:
    std::string msg_time_ = NULL;
    std::string msg_data_ = NULL;
    std::string msg_level_ = NULL;
    std::string msg_module_ = NULL;
    std::string msg_file_ = NULL;
    std::string msg_line_ = NULL;
    std::string msg_pid_ = NULL;
    std::string msg_tid = NULL;
};

class Log{
  public:
    Log();
    ~Log();
    void log(LogMsg*);
    void clear();
    

  private:
    int current_write_ = 0;
    int current_read_ = 0;
    LogMsg * log_buffer[MAX_SIZE_LOG];
    // static Log log_instance_;
};

using LogPtr = std::shared_ptr<Log>;

LogPtr log_instance;

LogPtr getLogInstance(){
  if (log_instance == NULL)
    log_instance = std::make_shared<Log>();
  return log_instance;
}

std::string getTime(){
  char buf[MAX_SIZE_LOG_TIME] = {};
  // char *     buf = (char*)malloc(sizeof(char)*MAX_SIZE_LOG_TIME);
  // if (buf == NULL){
  //   return std::string(LOG_GET_TIME_FAIL);
  // }
  time_t     now = time(0);
  struct tm  tstruct;
  tstruct = *localtime(&now);
  strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
  std::string ret = std::string(buf);
  // free(buf);
  return ret;
}
} // namespace message_infrastructure

#endif  // MESSAGE_INFRASTRUCTURE_LOGGING_H_
