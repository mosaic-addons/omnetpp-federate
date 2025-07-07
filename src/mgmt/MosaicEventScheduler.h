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

#ifndef MOSAICEVENTSCHEDULER_H_
#define MOSAICEVENTSCHEDULER_H_

#include <omnetpp.h>

#include "util/ClientServerChannel.h"
#include "msg/MosaicMobilityCmd_m.h"

namespace omnetpp_federate {
using namespace omnetpp;
using namespace ClientServerChannelSpace;

/**
 * Scheduler module for timeadvance nextevent mechanism
 * used in synchronization of mosaic and omnet++.
 *
 * @author rpr
 */
class MosaicEventScheduler : public cScheduler {

public:
  MosaicEventScheduler() = default;
  virtual ~MosaicEventScheduler() = default;

  virtual void startRun();
  virtual void endRun();
  virtual void setMgmtModule(cModule *mod);
  virtual void reportReceivedV2xMessage(cMessage *msg);

  virtual cEvent *guessNextEvent();
  virtual cEvent *takeNextEvent();
  virtual void putBackEvent(cEvent *event);

private:
  std::string m_host;
  int m_port;
  int m_cmdport;
  ClientServerChannel *m_ambassadorFederateChannel;
  ClientServerChannel *m_federateAmbassadorChannel;
  simtime_t m_startTime;
  simtime_t m_stopTime;
  simtime_t m_currentMaxSimTime;
  bool m_timeAdvancing = false;

  virtual void connectToAmbassador();
  virtual void reportNextEventToAmbassador(simtime_t nextSimTime);
  virtual void endTimeAdvance(simtime_t time);
  void receiveInteractions();

  void processShutDown();
  void processUpdateNode();
  MosaicMobilityCmd *processUpdateNodeCommand(
      const unsigned int numNodes, CSC_update_node_return &update_node_message,
      MobilityCommandType cmd_type, const bool newPosition = true);
  void processMsgSend();
  void processConfRadio();
  void processAdvanceTime();
};

} // namespace omnetpp_federate

#endif /* MOSAICEVENTSCHEDULER_H_ */
