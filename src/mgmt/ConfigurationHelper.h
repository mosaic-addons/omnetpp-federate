/*
 * Copyright (c) 2020 Fraunhofer FOKUS and others. All rights reserved.
 *
 * Contact: mosaic@fokus.fraunhofer.de
 *
 * This class is developed for the MOSAIC-NS-3 coupling.
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

#pragma once

#include <memory>
#include "msg/MosaicConfigurationCmd_m.h"
#include "msg/MosaicCommunicationCmd_m.h"
#include "inet/networklayer/contract/ipv4/Ipv4Address.h"
#include "ClientServerChannelMessages.pb.h"
#include <omnetpp.h>

namespace omnetpp_federate {

/**
 * Helper class providing modern C++ utilities for configuration and
 * communication Uses union-based message structures for type safety and
 * maintainability
 *
 * This class provides conversion functions from protobuf messages to OMNeT++
 * message structures, eliminating intermediate data structures and reducing
 * coupling.
 */
class ConfigurationHelper {
public:
  /**
   * Create WiFi configuration command from protobuf message
   *
   * @param message Protobuf WiFi configuration message
   * @param time Simulation time
   * @return Smart pointer to configured MosaicWifiConfigurationCmd
   */
  static std::unique_ptr<MosaicWifiConfigurationCmd> createWifiConfigurationCommand(
      const ClientServerChannelSpace::ConfigureWifiRadio &message,
      omnetpp::simtime_t time);

  /**
   * Create cellular configuration command from protobuf message
   *
   * @param message Protobuf cellular configuration message
   * @param time Simulation time
   * @return Smart pointer to configured MosaicCellularConfigurationCmd
   */
  static std::unique_ptr<MosaicCellularConfigurationCmd>
  createCellularConfigurationCommand(
      const ClientServerChannelSpace::ConfigureCellRadio &message,
      omnetpp::simtime_t time);

  /**
   * Create WiFi communication command from protobuf message
   *
   * @param message Protobuf WiFi message
   * @param time Simulation time
   * @return Smart pointer to configured MosaicCommunicationCmd
   */
  static std::unique_ptr<MosaicCommunicationCmd> createWifiCommunicationCommand(
      const ClientServerChannelSpace::SendWifiMessage &message,
      omnetpp::simtime_t time);

  /**
   * Create cellular communication command from protobuf message
   *
   * @param message Protobuf cellular message
   * @param time Simulation time
   * @return Smart pointer to configured MosaicCommunicationCmd
   */
  static std::unique_ptr<MosaicCommunicationCmd>
  createCellularCommunicationCommand(
      const ClientServerChannelSpace::SendCellMessage &message,
      omnetpp::simtime_t time);
};

} // namespace omnetpp_federate
