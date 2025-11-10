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

#include "MosaicProxyApp.h"

#include "common/binder/Binder.h"
#include "inet/common/Ptr.h"
#include "inet/common/packet/Message.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/packet/chunk/cPacketChunk.h"
#include "inet/common/socket/SocketTag_m.h"
#include "inet/networklayer/common/L3Address.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/ipv4/Ipv4InterfaceData.h"
#include "inet/networklayer/ipv4/Ipv4RoutingTable.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/RadioControlInfo_m.h"
#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211ControlInfo_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"

#include "msg/MosaicAppPacket_m.h"

namespace omnetpp_federate {

Define_Module(MosaicProxyApp);

void MosaicProxyApp::setExternalId(int id) { m_externalId = id; }

int MosaicProxyApp::getExternalId() const { return m_externalId; }

/**
 * Initialize method to bind a udp socket to this app layer.
 */
void MosaicProxyApp::initialize(int stage) {
  // because of IPAddressResolver, we need to wait until interfaces are
  // registered, address auto-assignment takes place etc.
  if (stage == 3) {
    EV << "Initializing MosaicUDPApp stage " << stage << std::endl;
    // this->numInitStages();
    localPort = par("localPort");
    destPort = par("destPort");
    maxProcDelay = par("maxProcDelay");

    socket.setOutputGate(gate("socketOut"));
    const char *la = par("localAddress");
    localAddress =
        *la ? inet::L3AddressResolver().resolve(la) : inet::L3Address();
    socket.bind(localAddress, localPort);
    socket.setBroadcast(true);
  } else if (stage == 5) {
    /** find our radios and save the references */
    if (inet::IInterfaceTable *ift = inet::L3AddressResolver().interfaceTableOf(
            this->getParentModule())) {
      if (ift) {
        for (int i = 0; i < ift->getNumInterfaces(); i++) {
          inet::NetworkInterface *tmpIe = ift->getInterface(i);
          if (tmpIe == nullptr || tmpIe->isLoopback()) {
            continue;
          }
          auto interfaceName = tmpIe->getFullName();
          EV << "Found interface: " << interfaceName << std::endl;
          if (std::strcmp(interfaceName, "wlan0") == 0) {
            ie0 = tmpIe;
          } else if (std::strcmp(interfaceName, "wlan1") == 0) {
            ie1 = tmpIe;
          } else if (std::strcmp(interfaceName, "cellularNic") == 0) {
            cellularIe = tmpIe;
          } else if (std::strncmp(interfaceName, "eth", 3) == 0) {
            ethIe = tmpIe;
          }
        }
      }
    }
    // Check if we have wireless interfaces or wired/cellular interfaces
    bool hasWirelessInterfaces = (ie0 != nullptr || ie1 != nullptr);
    bool hasWiredOrCellularInterface =
        (ethIe != nullptr || cellularIe != nullptr);

    if (!hasWirelessInterfaces && !hasWiredOrCellularInterface) {
      EV_ERROR << "MosaicProxyApp: No supported network interfaces found"
               << std::endl;
      setOperationalState(NOT_OPERATING);
      return;
    }

    if (hasWirelessInterfaces) {
      // Initialize wireless radios if available
      if ((radio0 = dynamic_cast<inet::physicallayer::Ieee80211Radio *>(
               ie0->getSubmodule("radio"))) == 0) {
        setOperationalState(NOT_OPERATING);
        return;
      }
      if ((radio1 = dynamic_cast<inet::physicallayer::Ieee80211Radio *>(
               ie1->getSubmodule("radio"))) == 0) {
        setOperationalState(NOT_OPERATING);
        return;
      }

      // Disconnect all radios initially
      numRadios = 2;
      connectRadios(0);
    } else {
      // For wired-only or cellular-only nodes, set numRadios to 0
      numRadios = 0;
      EV << "MosaicProxyApp operating in wired/cellular mode without wireless "
            "radios"
         << std::endl;
    }

    setOperationalState(OPERATING);
  }
}

/**
 * Simulate processing delay on application layer to avoid problem of dcf in mac
 * layer with completely synchronous message sending.
 */
void MosaicProxyApp::sendDelayedToUDP(omnetpp::cPacket *msg, int srcPort,
                                      const inet::Ipv4Address &destAddr,
                                      int destPort, double delay) {
  // channelId convention:
  //   -1 => non-WiFi (cellular or wired)
  //    n => WiFi channel id; must match radio0Channel or radio1Channel
  auto packet = omnetpp::check_and_cast<MosaicAppPacket *>(msg);

  // Allow messages if WiFi radios are available OR if it's a cellular message
  // with cellular interface OR if it's a wired interface
  bool canSend =
      (numRadios >= 1) || (packet->getChannelId() == -1 &&
                           (cellularIe != nullptr || ethIe != nullptr));
  if (!canSend) {
    EV_ERROR << "No radio/interface available for message "
             << packet->getMsgId() << " (channelId: " << packet->getChannelId()
             << ")" << std::endl;
    return;
  }

  // Create UDP Packet containing {msg}
  auto className = msg->getClassName();
  auto *udpPacket = new inet::Packet(
      !strncmp("inet::", className, 6) ? className + 6 : className);
  udpPacket->insertAtBack(inet::makeShared<inet::cPacketChunk>(msg));
  auto addresses = udpPacket->addTagIfAbsent<inet::L3AddressReq>();
  addresses->setSrcAddress(localAddress);
  addresses->setDestAddress(destAddr);
  udpPacket->addTagIfAbsent<inet::SocketReq>()->setSocketId(
      socket.getSocketId());
  udpPacket->addTagIfAbsent<inet::L4PortReq>()->setDestPort(destPort);

  int channelId = packet->getChannelId();
  // Decide egress interface for this packet.
  // Policy:
  //   - channelId == -1 => non-WiFi traffic (prefer cellular if present, else Ethernet)
  //   - channelId == radio0Channel => send via wlan0 (if radio0 is enabled)
  //   - channelId == radio1Channel => send via wlan1 (if radio1 is enabled)
  //   - otherwise => fall back to Ethernet if available
  const inet::NetworkInterface* outIf = nullptr;
  const char* outIfName = nullptr;

  // Non-WiFi traffic: prefer cellular when available
  if (channelId == -1 && cellularIe) {
    outIf = cellularIe; outIfName = "cellular";
  }
  // WiFi radio 0 (wlan0) selected if its channel matches and it's enabled
  else if (ie0 && numRadios > 0 && channelId == radio0Channel) {
    outIf = ie0; outIfName = "wlan0";
  }
  // WiFi radio 1 (wlan1) selected if its channel matches and it's enabled
  else if (ie1 && numRadios > 1 && channelId == radio1Channel) {
    outIf = ie1; outIfName = "wlan1";
  }
  // Fallback to Ethernet for non-WiFi or unmatched WiFi traffic
  else if (ethIe) {
    outIf = ethIe; outIfName = "ethernet";
  }

  // If no suitable interface was found, drop the packet with an error
  if (!outIf) {
    EV_ERROR << "No suitable interface for packet " << packet->getMsgId()
             << " (channelId=" << channelId << ")" << std::endl;
    delete udpPacket;
    return;
  }

  // Pin the packet to the chosen interface
  udpPacket->addTagIfAbsent<inet::InterfaceReq>()->setInterfaceId(outIf->getInterfaceId());

  EV << "Sending packet " << packet->getMsgId() << " via " << outIfName << " interface";
  if (channelId != -1)
    EV << " (channel " << channelId << ")";
  EV << " to " << destAddr << std::endl;

  sendDelayed(udpPacket, delay, gate("socketOut"));
}

/**
 * Method for sending of unreliable udp packets,
 * triggered from MosaicScenarioManager and hence from Mosaic.
 */
void MosaicProxyApp::sendPacket(omnetpp::cMessage *msg) {
  auto *packet = inet::check_and_cast<MosaicAppPacket *>(msg);
  auto destAddr = packet->getDestAddr();
  double delay = dblrand() * maxProcDelay;

  sendDelayedToUDP(omnetpp::check_and_cast<omnetpp::cPacket *>(msg->dup()),
                   localPort, destAddr, destPort, delay);
}

/**
 * Receive of udp packets and forwarding to Mosaic applications.
 */
void MosaicProxyApp::receivePacket(omnetpp::cMessage *msg) {
  EV << "MosaicUDP received packet: ";

  auto udp_packet = inet::check_and_cast<inet::Packet *>(msg);

  // Keep the shared chunk alive while accessing the inner cPacket
  auto chunk = udp_packet->popAtBack<inet::cPacketChunk>(udp_packet->getDataLength());
  if (!chunk) {
    EV_ERROR << "Received packet without cPacketChunk payload" << std::endl;
    return;
  }
  auto inner = chunk->getPacket();
  auto packet = omnetpp::check_and_cast<MosaicAppPacket *>(inner);

  EV << "srcNodeId " << packet->getNodeId() << ", msgId " << packet->getMsgId()
     << std::endl;
  packet->setNodeId(m_externalId);

  send(packet->dup(), gate("fedOut"));
}

/**
 * This method handles WiFi configuration messages.
 * If the number of radios is 0, no radio will be used
 */
void MosaicProxyApp::handleWifiConfiguration(MosaicWifiConfigurationCmd *cmd) {
  // channels are not alternatingly scheduled
  // For now we ignore georouting, so an interface is either there and fully
  // configured or removed

  if (cmd->getNumRadios() == 0) { // Disconnect radios from medium
    connectRadios(0);
    radio0Channel = -1;
    radio1Channel = -1;
    // It *should* not be necessary to change the IP address
    return;
  }
  if (ie0 == nullptr || ie1 == nullptr) {
    return; // somehow there are not the correct interfaces in the interface
            // table.
  }
  if (cmd->getNumRadios() == 1) {
    // connect the wanted number of radios
    connectRadios(1);
    if (cmd->getNumchannels0() == 1) { // simple case
                                       // set channel and power
      radio0->setChannelNumber(cmd->getChannel00());
      EV << "Setting channel " << cmd->getChannel00() << " on "
         << radio0->getFullName() << " ("
         << radio0->getTransmitter()->getInfoStringRepresentation() << ")"
         << std::endl;
      if (cmd->getPower0() > -1) {
        inet::W power = inet::W(cmd->getPower0()); // We get the power in mW
        radio0->setPower(power /
                         1000); // thus we have to divide by 1000 to get W
      }
      radio0Channel = cmd->getChannel00();
    } else if (cmd->getNumchannels0() == 2) { // switching case
                                              // Not yet implemented
    }

    // now configure the IP address
    ie0->getProtocolDataForUpdate<inet::Ipv4InterfaceData>()->setIPAddress(
        cmd->getIp0());
    ie0->getProtocolDataForUpdate<inet::Ipv4InterfaceData>()->setNetmask(
        cmd->getSubnet0());
    ie0->setBroadcast(true);
    return;
  }
  if (cmd->getNumRadios() == 2) {
    // connect the wanted number of radios
    connectRadios(2);
    if (cmd->getNumchannels0() == 1 && cmd->getNumchannels1() == 1) {
      // configure the first radio
      EV << "Setting channel " << cmd->getChannel00() << " on "
         << radio0->getFullName() << " ("
         << radio0->getTransmitter()->getInfoStringRepresentation() << ")"
         << std::endl;
      radio0->setChannelNumber(cmd->getChannel00());
      if (cmd->getPower0() > -1) {
        inet::W power0 = inet::W(cmd->getPower0() / 1000.0); // Convert mW to W
        radio0->setPower(power0);
      }
      radio0Channel = cmd->getChannel00();
      // configure the second radio
      EV << "Setting channel " << cmd->getChannel10() << " on "
         << radio1->getFullName() << " ("
         << radio1->getTransmitter()->getInfoStringRepresentation() << ")"
         << std::endl;
      radio1->setChannelNumber(cmd->getChannel10());
        inet::W power1 = inet::W(cmd->getPower1()); // We get the power in mW
        radio1->setPower(power1 /
                         1000); // thus we have to divide by 1000 to get W
      }
      radio1Channel = cmd->getChannel10();
    } else {
      return; // we do not support dual radio dual channel yet
    }

    // configure the IP addresses of the two radios
    ie0->getProtocolDataForUpdate<inet::Ipv4InterfaceData>()->setIPAddress(
        cmd->getIp0());
    ie0->getProtocolDataForUpdate<inet::Ipv4InterfaceData>()->setNetmask(
        cmd->getSubnet0());
    ie0->setBroadcast(true);
    ie1->getProtocolDataForUpdate<inet::Ipv4InterfaceData>()->setIPAddress(
        cmd->getIp1());
    ie1->getProtocolDataForUpdate<inet::Ipv4InterfaceData>()->setNetmask(
        cmd->getSubnet1());
    ie1->setBroadcast(true);
    return;
  }


/**
 * This method handles cellular configuration messages.
 */
void MosaicProxyApp::handleCellularConfiguration(
    MosaicCellularConfigurationCmd *cmd) {
  // Configure IP addresses if cellular interface exists
  if (cellularIe != nullptr) {
    if (cmd->getSubnet().isUnspecified()) {
      cmd->setSubnet(inet::Ipv4Address::makeNetmask(16));
    }
    cellularIe->getProtocolDataForUpdate<inet::Ipv4InterfaceData>()
        ->setIPAddress(cmd->getIp());
    cellularIe->getProtocolDataForUpdate<inet::Ipv4InterfaceData>()->setNetmask(
        cmd->getSubnet());

    // Get the binder and register the IP address
    simu5g::Binder *binder = omnetpp::check_and_cast<simu5g::Binder *>(
        getSimulation()->getModuleByPath("binder"));
    // Get parent module
    auto *parent = getParentModule();
    if (parent != nullptr) {

      // Register the IP address with the binder
      // Check the nr/macNodeId params
      auto nrMacId = simu5g::MacNodeId(parent->par("nrMacNodeId").intValue());
      auto macId = simu5g::MacNodeId(parent->par("macNodeId").intValue());

      EV_DEBUG << "Parent module macNodeId: " << macId
               << " nrMacNodeId: " << nrMacId << std::endl;
      simu5g::MacNodeId macNodeId;
      if (nrMacId != simu5g::NODEID_NONE)
        macNodeId = nrMacId;
      else if (macId != simu5g::NODEID_NONE)
        macNodeId = macId;
      else {
        EV_DEBUG << "No valid macNodeId or nrMacNodeId found for node with id: "
                 << parent->getId() << std::endl;
      }
      EV_DEBUG << "Got MAC node ID: " << macNodeId
               << " for node with id: " << parent->getId() << std::endl;
      // Associate the received IP address with the MAC node ID
      binder->setMacNodeId(cmd->getIp(), macNodeId);
      simu5g::MacNodeId test = binder->getNrMacNodeId(cmd->getIp());
      EV_DEBUG << "Registered IP address " << cmd->getIp()
               << " with MAC node ID: " << test << std::endl;
    }

    EV << "Configured cellular interface IP: " << cmd->getIp()
       << " subnet: " << cmd->getSubnet() << std::endl;
    // CellularConfigCommand also used for Ethernet configuration
  } else if (ethIe != nullptr) {
    if (cmd->getSubnet().isUnspecified()) {
      cmd->setSubnet(inet::Ipv4Address::makeNetmask(16));
    }
    ethIe->getProtocolDataForUpdate<inet::Ipv4InterfaceData>()->setIPAddress(
        cmd->getIp());
    ethIe->getProtocolDataForUpdate<inet::Ipv4InterfaceData>()->setNetmask(
        cmd->getSubnet());

    EV << "Configured Ethernet interface IP: " << cmd->getIp()
       << " subnet: " << cmd->getSubnet() << std::endl;
  } else {
    EV << "No cellular or Ethernet interface available to configure IP: "
       << cmd->getIp() << std::endl;
  }
}

/**
 * Base configuration handler - for backward compatibility or unknown types
 */
void MosaicProxyApp::handleConfiguration(MosaicConfigurationCmd *cmd) {
  // This method can be used for any base configuration logic
  // or as a fallback for unknown configuration types
  EV_WARN << "Received base MosaicConfigurationCmd - specific handler should "
             "be used"
          << std::endl;
}

/**
 * Sets the mode of the radios
 * 0 - no radio will be turned on
 * 1 - the first (wlan0) interfaces radio will be turned on
 * 2 - the first (wlan0) and the second (wlan1) radio will be turned on
 * @param number the number of radios
 */
void MosaicProxyApp::connectRadios(int number) {
  switch (number) {
  case 0:
    if (numRadios > 0) {
      radio0->setRadioMode(inet::physicallayer::IRadio::RADIO_MODE_OFF);
      if (numRadios > 1) {
        radio1->setRadioMode(inet::physicallayer::IRadio::RADIO_MODE_OFF);
      }
    }
    numRadios = 0;
    return;
  case 1:
    if (numRadios == 0) {
      radio0->setRadioMode(inet::physicallayer::IRadio::RADIO_MODE_RECEIVER);
    }
    if (numRadios == 2) {
      radio1->setRadioMode(inet::physicallayer::IRadio::RADIO_MODE_OFF);
    }
    numRadios = 1;
    return;
  case 2:
    if (numRadios < 2) {
      if (numRadios == 0) {
        radio0->setRadioMode(inet::physicallayer::IRadio::RADIO_MODE_RECEIVER);
      }
      radio1->setRadioMode(inet::physicallayer::IRadio::RADIO_MODE_RECEIVER);
    }
    numRadios = 2;
    return;
  default:
    return;
  }
}

void MosaicProxyApp::handleMessageWhenUp(omnetpp::cMessage *msg) {
  EV << "MosaicProxyApp received message: " << msg->getName() << std::endl;
  if (msg->arrivedOn("fedIn")) {
    // from federate
    if (auto wifiCmd = dynamic_cast<MosaicWifiConfigurationCmd *>(msg)) {
      handleWifiConfiguration(wifiCmd);
    } else if (auto cellCmd = dynamic_cast<MosaicCellularConfigurationCmd *>(msg)) {
      handleCellularConfiguration(cellCmd);
    } else if (auto baseCmd = dynamic_cast<MosaicConfigurationCmd *>(msg)) {
      handleConfiguration(baseCmd);
    } else if (numRadios > 0 || cellularIe != nullptr || ethIe != nullptr) { 
      // 
      sendPacket(msg);
    }
  } else if (msg->arrivedOn("socketIn")) {
    // from radio
    receivePacket(msg);
  }
  delete msg;
}

} // namespace omnetpp_federate
