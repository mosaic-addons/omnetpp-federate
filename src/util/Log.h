/*
 * Copyright (c) 2021 Fraunhofer FOKUS and others. All rights reserved.
 * Copyright (c) 2006,2007 INRIA
 *
 * Contact: mosaic@fokus.fraunhofer.de
 *
 * This code is developed for the MOSAIC-OMNeT++ coupling.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#ifndef __NS3LOG_H__
#define __NS3LOG_H__

#include <iostream>
#include <sstream>
#include <vector>

// default log stream
#ifndef LOG_OUT
#define LOG_OUT std::clog
#endif


// enum copied from ns-3.34, file: core/log.h
/**
 *  Logging severity classes and levels.
 */
enum LogLevel
{
  LOG_NONE           = 0x00000000, //!< No logging.

  LOG_ERROR          = 0x00000001, //!< Serious error messages only.
  LOG_LEVEL_ERROR    = 0x00000001, //!< LOG_ERROR and above.

  LOG_WARN           = 0x00000002, //!< Warning messages.
  LOG_LEVEL_WARN     = 0x00000003, //!< LOG_WARN and above.

  LOG_DEBUG          = 0x00000004, //!< Rare ad-hoc debug messages.
  LOG_LEVEL_DEBUG    = 0x00000007, //!< LOG_DEBUG and above.

  LOG_INFO           = 0x00000008, //!< Informational messages (e.g., banners).
  LOG_LEVEL_INFO     = 0x0000000f, //!< LOG_INFO and above.

  LOG_FUNCTION       = 0x00000010, //!< Function tracing.
  LOG_LEVEL_FUNCTION = 0x0000001f, //!< LOG_FUNCTION and above.

  LOG_LOGIC          = 0x00000020, //!< Control flow tracing within functions.
  LOG_LEVEL_LOGIC    = 0x0000003f, //!< LOG_LOGIC and above.

  LOG_ALL            = 0x0fffffff, //!< Print everything.
  LOG_LEVEL_ALL      = LOG_ALL,    //!< Print everything.

  LOG_PREFIX_FUNC    = 0x80000000, //!< Prefix all trace prints with function.
  LOG_PREFIX_TIME    = 0x40000000, //!< Prefix all trace prints with simulation time.
  LOG_PREFIX_NODE    = 0x20000000, //!< Prefix all trace prints with simulation node.
  LOG_PREFIX_LEVEL   = 0x10000000, //!< Prefix all trace prints with log level (severity).
  LOG_PREFIX_ALL     = 0xf0000000  //!< All prefixes.
};

// class copied from ns-3.34, file: core/log.h
/**
 * Insert `, ` when streaming function arguments.
 */
class ParameterLogger
{
  bool m_first;        //!< First argument flag, doesn't get `, `.
  std::ostream &m_os;  //!< Underlying output stream.

public:
  /**
   * Constructor.
   *
   * \param [in] os Underlying output stream.
   */
  ParameterLogger (std::ostream &os);

  /**
   * Write a function parameter on the output stream,
   * separating parameters after the first by `,` strings.
   *
   * \param [in] param The function parameter.
   * \return This ParameterLogger, so it's chainable.
   */
  template<typename T>
  ParameterLogger& operator<< (T param);

  /**
   * Overload for vectors, to print each element.
   *
   * \param [in] vector The vector of parameters
   * \return This ParameterLogger, so it's chainable.
   */
  template<typename T>
  ParameterLogger& operator<< (std::vector<T> vector);

};

// derived from ns-3.34, file: core/log.cc
template<typename T>
ParameterLogger &
ParameterLogger::operator<< (const T param)
{
  if (m_first)
    {
      m_os << (param);
      m_first = false;
    }
  else
    {
      m_os << ", " << (param);
    }
  return *this;
}


// define log level with default value or use defined preprocessor symbol LOG_LEVEL
#ifdef LOG_LEVEL
static int g_log_level = LOG_LEVEL;
#else
static int g_log_level = LOG_LEVEL_WARN;
#endif

void set_log_level_to_omnetpp_level ();

bool is_LOG_enabled (int level);

// define simplified LOG makros (compare to official ns3 logging macros)

#define LOG_CONDITION

#define LOG_COMPONENT_DEFINE(name)  \
  static std::string g_LOG_component_name = name;

// simplified prefix
#define LOG_APPEND_PREFIX \
  LOG_OUT << g_LOG_component_name << ": ";

// copied from ns-3.34, file: core/log.h
#define LOG_ERROR(msg) \
  NS_LOG (LOG_ERROR, msg)

// copied from ns-3.34, file: core/log.h
#define LOG_WARN(msg) \
  NS_LOG (LOG_WARN, msg)

// copied from ns-3.34, file: core/log.h
#define LOG_DEBUG(msg) \
  NS_LOG (LOG_DEBUG, msg)

// copied from ns-3.34, file: core/log.h
#define LOG_INFO(msg) \
  NS_LOG (LOG_INFO, msg)

// copied from ns-3.34, file: core/log.h
#define LOG_LOGIC(msg) \
  NS_LOG (LOG_LOGIC, msg)

// derived from ns-3.34, file: core/log.h
#define NS_LOG(level, msg)                                      \
  LOG_CONDITION                                                 \
  do {                                                          \
      if (is_LOG_enabled (level))                               \
        {                                                       \
          LOG_APPEND_PREFIX;                                    \
          LOG_OUT << msg << std::endl;                          \
        }                                                       \
    } while (false)

// derived from ns-3.34, file: core/log.h
#define LOG_FUNCTION(parameters)                                \
  LOG_CONDITION                                                 \
  do                                                            \
    {                                                           \
      if (is_LOG_enabled (LOG_FUNCTION))                        \
        {                                                       \
          LOG_APPEND_PREFIX;                                    \
          LOG_OUT << __FUNCTION__ << "(";                       \
          ParameterLogger (LOG_OUT) << parameters;              \
          LOG_OUT << ")" << std::endl;                          \
        }                                                       \
    }                                                           \
  while (false)

#endif