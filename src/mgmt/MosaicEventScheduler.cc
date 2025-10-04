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

#include "MosaicEventScheduler.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <signal.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include <omnetpp/clog.h>

#include "msg/MosaicAppPacket_m.h"
#include "msg/MosaicCommunicationCmd_m.h"
#include "msg/MosaicConfigurationCmd_m.h"

#include "inet/common/geometry/common/Coord.h"
#include "inet/networklayer/contract/ipv4/Ipv4Address.h"

namespace std {
std::ostream &operator<<(std::ostream &out,
                         omnetpp_federate::MobilityCommandType type) {
  switch (type) {
  case omnetpp_federate::MOBILITY_CMD_ADD_RADIO_VEH_NODE:
    out << "MOBILITY_CMD_ADD_RADIO_VEH_NODE";
    break;
  case omnetpp_federate::MOBILITY_CMD_ADD_RADIO_RSU_NODE:
    out << "MOBILITY_CMD_ADD_RADIO_RSU_NODE";
    break;
  case omnetpp_federate::MOBILITY_CMD_ADD_WIRED_NODE:
    out << "MOBILITY_CMD_ADD_WIRED_NODE";
    break;
  case omnetpp_federate::MOBILITY_CMD_ADD_NODE_B:
    out << "MOBILITY_CMD_ADD_NODE_B";
    break;
  case omnetpp_federate::MOBILITY_CMD_MOVE_NODES:
    out << "MOBILITY_CMD_MOVE_NODES";
    break;
  case omnetpp_federate::MOBILITY_CMD_REMOVE_NODE:
    out << "MOBILITY_CMD_REMOVE_NODE";
    break;
  }
  return out;
}
} // namespace std

namespace omnetpp_federate {
using namespace omnetpp;

/** Reference to scenario management module */
static cModule *mgmt;

Register_Class(MosaicEventScheduler);

Register_GlobalConfigOption(CFGID_MOSAICEVENTSCHEDULER_DEBUG,
                            "mosaiceventscheduler-debug", CFG_BOOL, "false",
                            "Switch for debugprints of scheduler.");

Register_GlobalConfigOption(CFGID_MOSAICEVENTSCHEDULER_HOST,
                            "mosaiceventscheduler-host", CFG_STRING, "NULL",
                            "Own hostname for connection with mosaic.");

Register_GlobalConfigOption(CFGID_MOSAICEVENTSCHEDULER_PORT,
                            "mosaiceventscheduler-port", CFG_INT, "0",
                            "Port for outchannel socket to mosaic.");

Register_GlobalConfigOption(CFGID_MOSAICCMD_PORT, "mosaiccmd-port", CFG_INT,
                            "0",
                            "Port for command channel socket from mosaic.");

void MosaicEventScheduler::startRun() {
  std::cout << "MosaicEventScheduler started" << endl;

  if (cSimulation::getActiveEnvir()->getConfig()->getAsBool(
          CFGID_MOSAICEVENTSCHEDULER_DEBUG)) {
    cLog::logLevel = LOGLEVEL_DEBUG;
  } else {
    cLog::logLevel = LOGLEVEL_INFO;
  }

  m_host = cSimulation::getActiveEnvir()->getConfig()->getAsString(
      CFGID_MOSAICEVENTSCHEDULER_HOST);
  m_port = cSimulation::getActiveEnvir()->getConfig()->getAsInt(
      CFGID_MOSAICEVENTSCHEDULER_PORT);
  m_cmdport = cSimulation::getActiveEnvir()->getConfig()->getAsInt(
      CFGID_MOSAICCMD_PORT);

  connectToAmbassador();
}

void MosaicEventScheduler::endRun() {
  static bool once = false;
  if (!once) {
    // toggle the once flag
    once = true;

    m_ambassadorFederateChannel.reset();
    m_federateAmbassadorChannel.reset();

    EV_DEBUG << "MosaicEventScheduler ended" << endl;
  }
}

/**
 * First - Connect to Ambassador.
 * Protocol is as follows:
 * 1 - Federate opens port A for writing (federateAmbassadorChannel)
 * 2 - Ambassador connects to port A
 * 3 - Federate opens another port B for reading (ambassadorFederateChannel)
 * 4 - Federate sends CMD_INIT message over federateAmbassadorChannel
 * 5 - Federate sends port B over federateAmbassadorChannel
 * 6 - Ambassador connects to port B
 *  next steps in receiveInteractions-Thread
 *
 * Second - Initialize the simulation with the ambassador
 *
 * 1 - Federate sends INIT-message containing start and endtime
 * 2 - Federate sets times and enables simulation
 * 3 - Federate sends SUCCESS-message
 *
 * If no init-message can be read, the federate sends END-message to ambassador
 */
void MosaicEventScheduler::connectToAmbassador() {
  m_federateAmbassadorChannel = std::make_unique<ClientServerChannel>();

  const int actPort =
      m_federateAmbassadorChannel->prepareConnection(m_host, m_port);
  if (actPort != m_port) {
    EV_DEBUG << "MosaicEventScheduler bound different port " << actPort
             << " instead of port " << m_port << endl;
  }
  // phenomenon: printing of the whole line fails non-deterministic leading to
  // wrong test result (fixme when reproduced)
  const auto outPortString =
      "MosaicEventScheduler connecting on OutPort=" + std::to_string(actPort) +
      " ";
  usleep(1000000);
  std::cout << std::flush << outPortString << std::flush << endl;
  EV_DEBUG << outPortString << endl;
  m_federateAmbassadorChannel->connect();

  m_ambassadorFederateChannel = std::make_unique<ClientServerChannel>();
  const int actCmdPort =
      m_ambassadorFederateChannel->prepareConnection(m_host, m_cmdport);
  std::cout << "MosaicEventScheduler connecting on CmdPort=" << actCmdPort
            << endl;
  m_federateAmbassadorChannel->writeCommand(CommandMessage_CommandType_INIT);
  m_federateAmbassadorChannel->writePort(actCmdPort);
  m_ambassadorFederateChannel->connect();

  EV_DEBUG << "MosaicEventScheduler connected to Ambassador" << endl;

  EV_DEBUG << "MosaicEventScheduler wait INIT" << endl;
  const auto command = m_ambassadorFederateChannel->readCommand();
  if (command == CommandMessage_CommandType_INIT) {
    // Initialize simulation times
    const auto message = m_ambassadorFederateChannel->readInitMessage();
    m_startTime =
        SimTime(message.simulation_start_time(), SimTimeUnit::SIMTIME_NS);
    m_stopTime =
        SimTime(message.simulation_end_time(), SimTimeUnit::SIMTIME_NS);
    EV_DEBUG << "MosaicEventScheduler simulation times: Start ("
             << m_startTime.str() << "s), Stop (" << m_stopTime.str() << "s)"
             << endl;

    m_currentMaxSimTime = m_startTime;

    EV_DEBUG << "MosaicEventScheduler successfully initialized" << endl;
    m_ambassadorFederateChannel->writeCommand(
        CommandMessage_CommandType_SUCCESS);
  } else {
    m_ambassadorFederateChannel->writeCommand(
        CommandMessage_CommandType_SHUT_DOWN);
    cRuntimeError("MosaicEventScheduler FAILURE (unexpected command %d)",
                  command);
  }
}

cEvent *MosaicEventScheduler::guessNextEvent() {
  return getSimulation()->getFES()->peekFirst();
}

void MosaicEventScheduler::putBackEvent(cEvent *event) {
  getSimulation()->getFES()->insert(event);
  getSimulation()->getFES()->sort();
}

cEvent *MosaicEventScheduler::takeNextEvent() {
  auto nextTime = simtime_t{0};
  auto curTime = getSimulation()->getSimTime();
  curTime = curTime - curTime.remainderForUnit(SimTimeUnit::SIMTIME_NS);
  do {
    if (!m_timeAdvancing) {
      receiveInteractions();
      continue;
    }
    if (getSimulation()->getFES()->isEmpty()) {
      if (curTime == m_stopTime) {
        EV_INFO << "Reached simulation stop time: " << m_stopTime
                << ". Ending simulation" << endl;
      }
      EV_DEBUG << "MosaicEventScheduler The FES is empty. Time: " << curTime
               << endl;
      endTimeAdvance(curTime);
      continue;
    }
    nextTime = getSimulation()->getFES()->peekFirst()->getArrivalTime();
    nextTime = nextTime - nextTime.remainderForUnit(SimTimeUnit::SIMTIME_NS);
    if (nextTime > m_currentMaxSimTime) {
      EV_DEBUG << "MosaicEventScheduler Next message lies further in the "
                  "future than we are allowed to simulate: "
               << nextTime.str() << endl;
      reportNextEventToAmbassador(nextTime);
      endTimeAdvance(curTime);
      continue;
    }
  } while (!m_timeAdvancing || getSimulation()->getFES()->isEmpty() ||
           nextTime > m_currentMaxSimTime);

  auto *event = getSimulation()->getFES()->removeFirst();
  if (event != nullptr && !event->isStale()) {
    return event;
  } else {
    return this->takeNextEvent();
  }
}

void MosaicEventScheduler::setMgmtModule(cModule *mod) { mgmt = mod; }

void MosaicEventScheduler::reportNextEventToAmbassador(simtime_t nextSimTime) {
  EV_DEBUG << "MosaicEventScheduler request NEXT_EVENT: t=" << nextSimTime.str()
           << endl;
  m_federateAmbassadorChannel->writeCommand(
      CommandMessage_CommandType_NEXT_EVENT);
  m_federateAmbassadorChannel->writeTimeMessage(
      nextSimTime.inUnit(SimTimeUnit::SIMTIME_NS));
}

void MosaicEventScheduler::endTimeAdvance(simtime_t time) {
  EV_DEBUG << "MosaicEventScheduler END time advance: t=" << time.str() << endl;
  m_federateAmbassadorChannel->writeCommand(CommandMessage_CommandType_END);
  m_federateAmbassadorChannel->writeTimeMessage(
      time.inUnit(SimTimeUnit::SIMTIME_NS));
  m_timeAdvancing = false;
}

void MosaicEventScheduler::reportReceivedV2xMessage(cMessage *msg) {
  auto *packet = check_and_cast<MosaicAppPacket *>(msg);
  EV_DEBUG << "MosaicEventScheduler report RECV_MESSAGE: t="
           << packet->getArrivalTime().str()
           << ", RecNodeId=" << packet->getNodeId()
           << ", MsgId=" << packet->getMsgId() << std::endl;
  if (packet){
    if (packet->getChannelId() == -1) { // This means a CELL message TODO: differentiate between wifi/cell message in a better way
      m_federateAmbassadorChannel->writeCommand(
          CommandMessage_CommandType_RECV_CELL_MSG);
      m_federateAmbassadorChannel->writeReceiveCellMessage(
          packet->getArrivalTime().inUnit(SimTimeUnit::SIMTIME_NS),
          packet->getNodeId(), packet->getMsgId());
    } else {
        m_federateAmbassadorChannel->writeCommand(
      CommandMessage_CommandType_RECV_WIFI_MSG);
  m_federateAmbassadorChannel->writeReceiveWifiMessage(
      packet->getArrivalTime().inUnit(SimTimeUnit::SIMTIME_NS),
      packet->getNodeId(), packet->getMsgId(),
      static_cast<RadioChannel>(packet->getChannelId()), 0);
  // rssi and channel number are not reported

    }
  }

}

void MosaicEventScheduler::processShutDown() {
  EV_DEBUG << "MosaicEventScheduler received shut down command" << endl;
  auto *finMessage = new cMessage("MosaicFinishCmd", 22);
  finMessage->setTimestamp(m_currentMaxSimTime);
  finMessage->setArrivalTime(m_currentMaxSimTime);
  finMessage->setArrival(mgmt->getId(), -1);
  putBackEvent(finMessage);
}

void MosaicEventScheduler::processAddNode() {
  const auto message = m_ambassadorFederateChannel->readAddNode();

  const auto time = SimTime(message.time(), SimTimeUnit::SIMTIME_NS);

  auto *cmdMessage = new MosaicMobilityCmd("MosaicMobilityCmd");
  cmdMessage->setNodeIdArraySize(1);
  cmdMessage->setNodeId(0, message.node_id());

  if (message.type() == AddNode_NodeType_RADIO_NODE) {
    cmdMessage->setCmdType(MOBILITY_CMD_ADD_RADIO_VEH_NODE);
    EV_DEBUG << "MosaicEventScheduler received ADD_RADIO_NODE command: "
             << time.str() << endl;
  } else if (message.type() == AddNode_NodeType_WIRED_NODE) {
    cmdMessage->setCmdType(MOBILITY_CMD_ADD_WIRED_NODE);
    EV_DEBUG << "MosaicEventScheduler received ADD_WIRED_NODE command: "
             << time.str() << endl;
  } else if (message.type() == AddNode_NodeType_NODE_B) {
    cmdMessage->setCmdType(MOBILITY_CMD_ADD_NODE_B);
    EV_DEBUG << "MosaicEventScheduler received ADD_NODE_B command: "
             << time.str() << endl;
  }

  cmdMessage->setPositionArraySize(1);
  inet::Coord coord;
  coord.x = message.x();
  coord.y = message.y();
  coord.z = message.z();
  cmdMessage->setPosition(0, coord);

  cmdMessage->setTimestamp(time);
  cmdMessage->setArrivalTime(time);
  cmdMessage->setArrival(mgmt->getId(), -1);

  putBackEvent(cmdMessage);

  EV_DEBUG << "MosaicEventScheduler finished processing of command" << endl;
  m_ambassadorFederateChannel->writeCommand(CommandMessage_CommandType_SUCCESS);
}

void MosaicEventScheduler::processUpdateNode() {
  const auto message = m_ambassadorFederateChannel->readUpdateNode();

  const auto time = SimTime(message.time(), SimTimeUnit::SIMTIME_NS);
  const unsigned int numNodes = message.properties_size();

  EV_DEBUG << "MosaicEventScheduler received UPDATE_NODE command: "
           << time.str() << " for " << numNodes << " nodes" << endl;

  auto *cmdMessage = new MosaicMobilityCmd("MosaicMobilityCmd");
  cmdMessage->setCmdType(MOBILITY_CMD_MOVE_NODES);
  cmdMessage->setNodeIdArraySize(numNodes);
  cmdMessage->setPositionArraySize(numNodes);

  for (size_t i = 0; i < message.properties_size(); i++) {
    const auto node_data = message.properties(i);
    EV_DEBUG << "MosaicEventScheduler " << MOBILITY_CMD_MOVE_NODES << ": "
             << node_data.id() << " at position " << node_data.x() << ","
             << node_data.y() << std::endl;
    cmdMessage->setNodeId(i, node_data.id());
    inet::Coord coord;
    coord.x = node_data.x();
    coord.y = node_data.y();
    coord.z = node_data.z();
    cmdMessage->setPosition(i, coord);
  }

  cmdMessage->setTimestamp(time);
  cmdMessage->setArrivalTime(time);
  cmdMessage->setArrival(mgmt->getId(), -1);

  putBackEvent(cmdMessage);

  EV_DEBUG << "MosaicEventScheduler finished processing of command" << endl;
  m_ambassadorFederateChannel->writeCommand(CommandMessage_CommandType_SUCCESS);
}

void MosaicEventScheduler::processRemoveNode() {
  const auto message = m_ambassadorFederateChannel->readRemoveNode();

  const auto time = SimTime(message.time(), SimTimeUnit::SIMTIME_NS);

  EV_DEBUG << "MosaicEventScheduler received REMOVE_NODE command: "
           << time.str() << endl;

  auto *cmdMessage = new MosaicMobilityCmd("MosaicMobilityCmd");
  cmdMessage->setNodeIdArraySize(1);
  cmdMessage->setNodeId(0, message.node_id());
  cmdMessage->setCmdType(MOBILITY_CMD_REMOVE_NODE);

  cmdMessage->setPositionArraySize(0);

  cmdMessage->setTimestamp(time);
  cmdMessage->setArrivalTime(time);
  cmdMessage->setArrival(mgmt->getId(), -1);

  putBackEvent(cmdMessage);

  EV_DEBUG << "MosaicEventScheduler finished processing of command" << endl;
  m_ambassadorFederateChannel->writeCommand(CommandMessage_CommandType_SUCCESS);
}

void MosaicEventScheduler::processSendWifiMsg() {
  const auto send_message = m_ambassadorFederateChannel->readSendWifiMessage();
  const simtime_t time(send_message.time(), SimTimeUnit::SIMTIME_NS);

  EV_DEBUG << "MosaicEventScheduler.processSendWifiMsg() received time: "
           << time.str() << endl;

  // Create communication command using helper
  auto comMessage =
      ConfigurationHelper::createWifiCommunicationCommand(send_message, time);

  EV_DEBUG << "MosaicEventScheduler SEND_MESSAGE: from="
           << comMessage->getNodeId()
           << ", destAddress: " << comMessage->getDestAddr()
           << ", id=" << comMessage->getMsgId() << ", prot=udp" << std::endl;

  comMessage->setArrival(mgmt->getId(), -1);
  putBackEvent(comMessage.release());

  EV_DEBUG << "MosaicEventScheduler finished processing SEND_WIFI_MSG" << endl;
  m_ambassadorFederateChannel->writeCommand(CommandMessage_CommandType_SUCCESS);
}

void MosaicEventScheduler::processConfWifiRadio() {
  const auto config_message =
      m_ambassadorFederateChannel->readConfigureWifiRadio();
  const simtime_t time(config_message.time(), SimTimeUnit::SIMTIME_NS);

  EV_DEBUG << "MosaicEventScheduler received CONF_WIFI_RADIO command: "
           << time.str() << endl;

  // Create configuration command using modern helper
  auto confMessage =
      ConfigurationHelper::createWifiConfigurationCommand(config_message, time);

  EV_DEBUG << "MosaicEventScheduler CONF_RADIO: at " << time
           << " from=" << confMessage->getNodeId()
           << ", id=" << confMessage->getMsgId() << std::endl;

  confMessage->setArrival(mgmt->getId(), -1);
  putBackEvent(confMessage.release());

  EV_DEBUG << "MosaicEventScheduler finished processing CONF_WIFI_RADIO"
           << endl;
  m_ambassadorFederateChannel->writeCommand(CommandMessage_CommandType_SUCCESS);
}

void MosaicEventScheduler::processSendCellMsg() {
  const auto send_message = m_ambassadorFederateChannel->readSendCellMessage();
  const simtime_t time(send_message.time(), SimTimeUnit::SIMTIME_NS);

  EV_DEBUG << "MosaicEventScheduler.processSendCellMsg() received time: "
           << time.str() << endl;

  // Create communication command using helper
  auto comMessage = ConfigurationHelper::createCellularCommunicationCommand(
      send_message, time);

  EV_DEBUG << "MosaicEventScheduler SEND_CELL_MESSAGE: from="
          << comMessage->getNodeId()
          << ", destAddress: " << comMessage->getDestAddr()
          << ", id=" << comMessage->getMsgId() << ", prot=cellular"
          << std::endl;

  comMessage->setArrival(mgmt->getId(), -1);
  putBackEvent(comMessage.release());

  EV_DEBUG << "MosaicEventScheduler finished processing SEND_CELL_MSG" << endl;
  m_ambassadorFederateChannel->writeCommand(CommandMessage_CommandType_SUCCESS);
}

void MosaicEventScheduler::processConfCellRadio() {
  const auto config_message =
      m_ambassadorFederateChannel->readConfigureCellRadio();
  const simtime_t time(config_message.time(), SimTimeUnit::SIMTIME_NS);

  EV_DEBUG << "MosaicEventScheduler received CONF_CELL_RADIO command: "
           << time.str() << endl;

  // Create configuration command using modern helper
  auto confMessage = ConfigurationHelper::createCellularConfigurationCommand(
      config_message, time);

  EV_DEBUG << "MosaicEventScheduler CONF_CELL_RADIO: at " << time
           << " from=" << confMessage->getNodeId()
           << ", id=" << confMessage->getMsgId() << std::endl;

  confMessage->setArrival(mgmt->getId(), -1);
  putBackEvent(confMessage.release());

  EV_DEBUG << "MosaicEventScheduler finished processing CONF_CELL_RADIO"
           << endl;
  m_ambassadorFederateChannel->writeCommand(CommandMessage_CommandType_SUCCESS);
}

void MosaicEventScheduler::processAdvanceTime() {
  const int64_t newMaxTime = m_ambassadorFederateChannel->readTimeMessage();
  m_currentMaxSimTime = SimTime(newMaxTime, SimTimeUnit::SIMTIME_NS);
  m_timeAdvancing = true;
  

  
  EV_DEBUG << "MosaicEventScheduler ADVANCE_TIME: " << m_currentMaxSimTime
           << std::endl;
}

void MosaicEventScheduler::receiveInteractions() {
  EV_DEBUG << "MosaicEventScheduler wait new command" << std::endl;
  const auto command = m_ambassadorFederateChannel->readCommand();
  EV_DEBUG << "MosaicEventScheduler received command: " << command << std::endl;

  switch (command) {
  case CommandMessage_CommandType_SHUT_DOWN:
    processShutDown();
    break;
  case CommandMessage_CommandType_ADD_NODE:
    processAddNode();
    break;
  case CommandMessage_CommandType_UPDATE_NODE:
    processUpdateNode();
    break;
  case CommandMessage_CommandType_REMOVE_NODE:
    processRemoveNode();
    break;
  case CommandMessage_CommandType_SEND_WIFI_MSG:
    processSendWifiMsg();
    break;
  case CommandMessage_CommandType_CONF_WIFI_RADIO:
    processConfWifiRadio();
    break;
  case CommandMessage_CommandType_SEND_CELL_MSG:
    processSendCellMsg();
    break;
  case CommandMessage_CommandType_CONF_CELL_RADIO:
    processConfCellRadio();
    break;
  case CommandMessage_CommandType_ADVANCE_TIME:
    processAdvanceTime();
    break;
  default: {
    m_ambassadorFederateChannel->writeCommand(CommandMessage_CommandType_END);
    EV_DEBUG << "MosaicEventScheduler Received unknown command from "
                "ambassador, ending"
             << std::endl;
    m_timeAdvancing = true;
    cRuntimeError("MosaicEventScheduler FAILURE (received unknown command %d)",
                  command);
  }
  }
}

} // namespace omnetpp_federate
