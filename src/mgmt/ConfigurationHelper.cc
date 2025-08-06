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

#include "ConfigurationHelper.h"
#include "inet/common/Units.h"

namespace omnetpp_federate {

std::unique_ptr<MosaicWifiConfigurationCmd>
ConfigurationHelper::createWifiConfigurationCommand(
    const ClientServerChannelSpace::ConfigureWifiRadio &message,
    omnetpp::simtime_t time) {

  auto confMessage =
      std::make_unique<MosaicWifiConfigurationCmd>("MosaicWifiConfigurationCmd");
  confMessage->setTimestamp(time);
  confMessage->setArrivalTime(time);
  confMessage->setMsgId(message.message_id());
  confMessage->setNodeId(message.node_id());

  // Set number of radios based on protobuf message
  if (message.radio_number() ==
      ClientServerChannelSpace::ConfigureWifiRadio_RadioNumber_SINGLE_RADIO) {
    confMessage->setNumRadios(1);
  } else if (message.radio_number() ==
             ClientServerChannelSpace::
                 ConfigureWifiRadio_RadioNumber_DUAL_RADIO) {
    confMessage->setNumRadios(2);
  } else {
    confMessage->setNumRadios(0);
    return confMessage; // No radio configuration needed
  }

  // Configure primary radio (radio 0)
  if (confMessage->getNumRadios() >= 1) {
    const auto &primaryConfig = message.primary_radio_configuration();
    confMessage->setTurnedOn0(primaryConfig.receiving_messages());
    confMessage->setIp0(inet::Ipv4Address(primaryConfig.ip_address()));
    confMessage->setSubnet0(inet::Ipv4Address(primaryConfig.subnet_address()));
    confMessage->setPower0(primaryConfig.transmission_power());

    if (primaryConfig.radio_mode() ==
        ClientServerChannelSpace::
            ConfigureWifiRadio_RadioConfiguration_RadioMode_SINGLE_CHANNEL) {
      confMessage->setNumchannels0(1);
      confMessage->setChannel00(primaryConfig.primary_radio_channel());
    } else if (
        primaryConfig.radio_mode() ==
        ClientServerChannelSpace::
            ConfigureWifiRadio_RadioConfiguration_RadioMode_DUAL_CHANNEL) {
      confMessage->setNumchannels0(2);
      confMessage->setChannel00(primaryConfig.primary_radio_channel());
      confMessage->setChannel01(primaryConfig.secondary_radio_channel());
    }
  }

  // Configure secondary radio (radio 1) for dual radio setups
  if (confMessage->getNumRadios() >= 2 &&
      message.has_secondary_radio_configuration()) {
    const auto &secondaryConfig = message.secondary_radio_configuration();
    confMessage->setTurnedOn1(secondaryConfig.receiving_messages());
    confMessage->setIp1(inet::Ipv4Address(secondaryConfig.ip_address()));
    confMessage->setSubnet1(inet::Ipv4Address(secondaryConfig.subnet_address()));
    confMessage->setPower1(secondaryConfig.transmission_power());

    if (secondaryConfig.radio_mode() ==
        ClientServerChannelSpace::
            ConfigureWifiRadio_RadioConfiguration_RadioMode_SINGLE_CHANNEL) {
      confMessage->setNumchannels1(1);
      confMessage->setChannel10(secondaryConfig.primary_radio_channel());
    } else if (
        secondaryConfig.radio_mode() ==
        ClientServerChannelSpace::
            ConfigureWifiRadio_RadioConfiguration_RadioMode_DUAL_CHANNEL) {
      confMessage->setNumchannels1(2);
      confMessage->setChannel10(secondaryConfig.primary_radio_channel());
      confMessage->setChannel11(secondaryConfig.secondary_radio_channel());
    }
  }

  return confMessage;
}

std::unique_ptr<MosaicCellularConfigurationCmd>
ConfigurationHelper::createCellularConfigurationCommand(
    const ClientServerChannelSpace::ConfigureCellRadio &message,
    omnetpp::simtime_t time) {

  auto confMessage =
      std::make_unique<MosaicCellularConfigurationCmd>("MosaicCellularConfigurationCmd");
  confMessage->setTimestamp(time);
  confMessage->setArrivalTime(time);
  confMessage->setMsgId(0); // Cellular config doesn't have message ID in protobuf
  confMessage->setNodeId(message.node_id());

  // Set cellular configuration parameters
  confMessage->setIp(inet::Ipv4Address(message.ip_address()));
  confMessage->setSubnet(inet::Ipv4Address(message.subnet_address()));

  return confMessage;
}

std::unique_ptr<MosaicCommunicationCmd>
ConfigurationHelper::createWifiCommunicationCommand(
    const ClientServerChannelSpace::SendWifiMessage &message,
    omnetpp::simtime_t time) {

  auto comMessage =
      std::make_unique<MosaicCommunicationCmd>("MosaicCommunicationCmd");
  comMessage->setCmdType(COMMUNICATION_CMD_SEND_WIFI_MESSAGE);
  comMessage->setTimestamp(time);
  comMessage->setArrivalTime(time);
  comMessage->setNodeId(message.node_id());
  comMessage->setChannelId(message.channel_id());
  comMessage->setMsgId(message.message_id());
  comMessage->setLength(inet::B(message.length()));

  // Set destination address (only topological addressing supported for now)
  comMessage->setDestAddr(
      inet::Ipv4Address(message.topological_address().ip_address()));
  comMessage->setTtl(message.topological_address().ttl());

  return comMessage;
}

std::unique_ptr<MosaicCommunicationCmd>
ConfigurationHelper::createCellularCommunicationCommand(
    const ClientServerChannelSpace::SendCellMessage &message,
    omnetpp::simtime_t time) {

  auto comMessage =
      std::make_unique<MosaicCommunicationCmd>("MosaicCommunicationCmd");
  comMessage->setCmdType(COMMUNICATION_CMD_SEND_CELL_MESSAGE);
  comMessage->setTimestamp(time);
  comMessage->setArrivalTime(time);
  comMessage->setNodeId(message.node_id());
  comMessage->setChannelId(0); // Cellular doesn't use channel ID
  comMessage->setMsgId(message.message_id());
  comMessage->setLength(inet::B(message.length()));

  // Set destination address (only topological addressing supported)
  comMessage->setDestAddr(
      inet::Ipv4Address(message.topological_address().ip_address()));
  comMessage->setTtl(0); // TTL not used in cellular

  return comMessage;
}

} // namespace omnetpp_federate
