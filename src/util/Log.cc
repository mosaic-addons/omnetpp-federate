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
 
 #include "Log.h"

bool is_LOG_enabled(int level) {
  if (g_log_level & level)
    return true;
  else
    return false;
}


// copied from ns-3.34, file: core/log.cc
ParameterLogger::ParameterLogger (std::ostream &os)
  : m_first (true),
    m_os (os)
{}

// copied from ns-3.34, file: core/log.cc
template<>
ParameterLogger &
ParameterLogger::operator<< <std::string> (const std::string param)
{
  if (m_first)
    {
      m_os << "\"" << param << "\"";
      m_first = false;
    }
  else
    {
      m_os << ", \"" << param << "\"";
    }
  return *this;
}

// copied from ns-3.34, file: core/log.cc
template<>
ParameterLogger &
ParameterLogger::operator<< <const char *> (const char * param)
{
  (*this) << std::string (param);
  return *this;
}