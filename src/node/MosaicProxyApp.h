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

#ifndef OMNETPP_FEDERATE_NODE_MOSAICPROXYAPP_H_
#define OMNETPP_FEDERATE_NODE_MOSAICPROXYAPP_H_

#include <omnetpp.h>

#include "msg/MosaicConfigurationCmd_m.h"

#include "inet/applications/base/ApplicationBase.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/networklayer/common/NetworkInterface.h"
#include "inet/networklayer/contract/ipv4/Ipv4Address.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "inet/transportlayer/udp/UdpHeader_m.h"

#include "inet/physicallayer/wireless/ieee80211/packetlevel/Ieee80211Radio.h"

namespace omnetpp_federate {

class INET_API MosaicProxyApp : public inet::ApplicationBase {
public:
  MosaicProxyApp() = default;
  virtual ~MosaicProxyApp() = default;

  void setExternalId(int id);
  int getExternalId() const;

  virtual void initialize(int stage);
  void sendDelayedToUDP(omnetpp::cPacket *msg, int srcPort,
                        const inet::Ipv4Address &destAddr, int destPort,
                        double delay);
  void sendPacket(omnetpp::cMessage *msg);
  void receivePacket(omnetpp::cMessage *msg);
  virtual void handleConfiguration(MosaicConfigurationCmd *cmd);
  void connectRadios(int number);
  virtual void handleMessageWhenUp(omnetpp::cMessage *msg);

private:
  int m_externalId;
  inet::L3Address localAddress;
  int localPort;
  int destPort;
  inet::UdpSocket socket;
  double maxProcDelay;
  /** determines the number of radios, 0 means no radio and thus no message
   * sending. 1 means sending messages only on wlan0; 2 means that messages will
   * be sent on their according radio*/
  int numRadios = 0;

  inet::physicallayer::Ieee80211Radio *radio0 = nullptr;
  inet::physicallayer::Ieee80211Radio *radio1 = nullptr;
  inet::NetworkInterface *ie0 = nullptr;
  inet::NetworkInterface *ie1 = nullptr;
  int radio0Channel;
  int radio1Channel;

protected:
  virtual void handleStartOperation(inet::LifecycleOperation *operation) {
    // nop
  };
  virtual void handleStopOperation(inet::LifecycleOperation *operation) {
    // nop
  };
  virtual void handleCrashOperation(inet::LifecycleOperation *operation) {
    // nop
  };
};

} // namespace omnetpp_federate

#endif
