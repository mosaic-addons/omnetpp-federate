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

#include <unistd.h>
#include <signal.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <chrono>

#include <omnetpp/clog.h>

#include "msg/MosaicCommunicationCmd_m.h"
#include "msg/MosaicConfigurationCmd_m.h"
#include "msg/MosaicAppPacket_m.h"

#include "inet/networklayer/contract/ipv4/Ipv4Address.h"
#include "inet/common/geometry/common/Coord.h"

namespace std {
std::ostream& operator<< ( std::ostream& out, omnetpp_federate::MobilityCommandType type ) {
    switch (type) {
        case omnetpp_federate::MOBILITY_CMD_MOVE_NODES: out << "MOBILITY_CMD_MOVE_NODES"; break;
        case omnetpp_federate::MOBILITY_CMD_ADD_NODES: out << "MOBILITY_CMD_ADD_NODES"; break;
        case omnetpp_federate::MOBILITY_CMD_ADD_RSU_NODES: out << "MOBILITY_CMD_ADD_RSU_NODES"; break;
        case omnetpp_federate::MOBILITY_CMD_REMOVE_NODES: out << "MOBILITY_CMD_REMOVE_NODES"; break;
    }
    return out;
}
}

namespace omnetpp_federate {
using namespace omnetpp;

/** Reference to scenario management module */
static cModule* mgmt;

Register_Class(MosaicEventScheduler);

Register_GlobalConfigOption(CFGID_MOSAICEVENTSCHEDULER_DEBUG, "mosaiceventscheduler-debug",
        CFG_BOOL, "false", "Switch for debugprints of scheduler.");

Register_GlobalConfigOption(CFGID_MOSAICEVENTSCHEDULER_HOST, "mosaiceventscheduler-host",
        CFG_STRING, "NULL", "Own hostname for connection with mosaic.");

Register_GlobalConfigOption(CFGID_MOSAICEVENTSCHEDULER_PORT, "mosaiceventscheduler-port",
        CFG_INT, "0", "Port for outchannel socket to mosaic.");

Register_GlobalConfigOption(CFGID_MOSAICCMD_PORT, "mosaiccmd-port",
        CFG_INT, "0", "Port for command channel socket from mosaic.");

void MosaicEventScheduler::startRun() {
    std::cout << "MosaicEventScheduler started" << endl;

    if (cSimulation::getActiveEnvir()->getConfig()->getAsBool(CFGID_MOSAICEVENTSCHEDULER_DEBUG)) {
        cLog::logLevel = LOGLEVEL_DEBUG;
    } else {
        cLog::logLevel = LOGLEVEL_INFO;
    }

    m_host = cSimulation::getActiveEnvir()->getConfig()->getAsString(CFGID_MOSAICEVENTSCHEDULER_HOST);
    m_port = cSimulation::getActiveEnvir()->getConfig()->getAsInt(CFGID_MOSAICEVENTSCHEDULER_PORT);
    m_cmdport = cSimulation::getActiveEnvir()->getConfig()->getAsInt(CFGID_MOSAICCMD_PORT);

    connectToAmbassador();
}

void MosaicEventScheduler::endRun() {
    static bool once = false;
    if (!once) {
        // toggle the once flag
        once = true;

        delete m_ambassadorFederateChannel;
        delete m_federateAmbassadorChannel;

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
    m_federateAmbassadorChannel = new ClientServerChannel();

    const int actPort = m_federateAmbassadorChannel->prepareConnection(m_host, m_port);
    if (actPort != m_port) {
        EV_DEBUG << "MosaicEventScheduler bound different port " << actPort
                << " instead of port " << m_port << endl;
    }
    // phenomenon: printing of the whole line fails non-deterministic leading to wrong test result (fixme when reproduced)
    const std::string outPortString
    = "MosaicEventScheduler connecting on OutPort=" + std::to_string(actPort) + " ";
    usleep(1000000);
    std::cout << std::flush << outPortString << std::flush << endl;
    EV_DEBUG << outPortString << endl;
    m_federateAmbassadorChannel->connect();

    m_ambassadorFederateChannel = new ClientServerChannel();
    const int actCmdPort = m_ambassadorFederateChannel->prepareConnection(m_host, m_cmdport);
    std::cout << "MosaicEventScheduler connecting on CmdPort=" << actCmdPort << endl;
    m_federateAmbassadorChannel->writeCommand(CMD_INIT);
    m_federateAmbassadorChannel->writePort(actCmdPort);
    m_ambassadorFederateChannel->connect();

    EV_DEBUG << "MosaicEventScheduler connected to Ambassador" << endl;

    EV_DEBUG << "MosaicEventScheduler wait INIT" << endl;
    CMD command = m_ambassadorFederateChannel->readCommand();
    if (command == CMD_INIT) {
        // Initialize simulation times
        CSC_init_return init_message;
        m_ambassadorFederateChannel->readInit(init_message);
        m_startTime = SimTime(init_message.start_time, SimTimeUnit::SIMTIME_NS);
        m_stopTime = SimTime(init_message.end_time, SimTimeUnit::SIMTIME_NS);
        EV_DEBUG << "MosaicEventScheduler simulation times: Start (" << m_startTime.str()
                         << "s), Stop (" << m_stopTime.str() << "s)" << endl;

        m_currentMaxSimTime = m_startTime;

        EV_DEBUG << "MosaicEventScheduler successfully initialized" << endl;
        m_ambassadorFederateChannel->writeCommand(CMD_SUCCESS);
    } else {
        m_ambassadorFederateChannel->writeCommand(CMD_END);
        cRuntimeError("MosaicEventScheduler FAILURE (unexpected command %d)", command);
    }
}

cEvent*  MosaicEventScheduler::guessNextEvent() {
    return getSimulation()->getFES()->peekFirst();
}

void MosaicEventScheduler::putBackEvent(cEvent* event) {
    getSimulation()->getFES()->insert(event);
    getSimulation()->getFES()->sort();
}

cEvent* MosaicEventScheduler::takeNextEvent() {
    simtime_t nextTime = 0;
    simtime_t curTime = getSimulation()->getSimTime();
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
            EV_DEBUG << "MosaicEventScheduler The FES is empty. Time: " << curTime << endl;
            endTimeAdvance(curTime);
            continue;
        }
        nextTime = getSimulation()->getFES()->peekFirst()->getArrivalTime();
        nextTime = nextTime - nextTime.remainderForUnit(SimTimeUnit::SIMTIME_NS);
        if (nextTime > m_currentMaxSimTime) {
            EV_DEBUG << "MosaicEventScheduler Next message lies further in the future than we are allowed to simulate: "
                    << nextTime.str()  << endl;
            reportNextEventToAmbassador(nextTime);
            endTimeAdvance(curTime);
            continue;
        }
    } while (!m_timeAdvancing || getSimulation()->getFES()->isEmpty() || nextTime > m_currentMaxSimTime);

    cEvent* event = getSimulation()->getFES()->removeFirst();
    if (event != NULL && !event->isStale()) {
        return event;
    } else {
        return this->takeNextEvent();
    }
}

void MosaicEventScheduler::setMgmtModule(cModule *mod) {
    mgmt = mod;
}

void MosaicEventScheduler::reportNextEventToAmbassador(simtime_t nextSimTime) {
    EV_DEBUG << "MosaicEventScheduler request NEXT_EVENT: t=" << nextSimTime.str() << endl;
    m_federateAmbassadorChannel->writeCommand(CMD_NEXT_EVENT);
    m_federateAmbassadorChannel->writeTimeMessage(nextSimTime.inUnit(SimTimeUnit::SIMTIME_NS));
}

void MosaicEventScheduler::endTimeAdvance(simtime_t time) {
    EV_DEBUG << "MosaicEventScheduler END time advance: t=" << time.str() << endl;
    m_federateAmbassadorChannel->writeCommand(CMD_END);
    m_federateAmbassadorChannel->writeTimeMessage(time.inUnit(SimTimeUnit::SIMTIME_NS));
    m_timeAdvancing = false;
}

void MosaicEventScheduler::reportReceivedV2xMessage(cMessage *msg) {
    MosaicAppPacket *packet = check_and_cast<MosaicAppPacket *>(msg);
    EV_DEBUG << "MosaicEventScheduler report RECV_MESSAGE: t=" << packet->getArrivalTime().str()
                   << ", RecNodeId=" << packet->getNodeId() << ", MsgId=" << packet->getMsgId() << std::endl;

    m_federateAmbassadorChannel->writeCommand(CMD_MSG_RECV);
    m_federateAmbassadorChannel->writeReceiveMessage(packet->getArrivalTime().inUnit(SimTimeUnit::SIMTIME_NS),
            packet->getNodeId(), packet->getMsgId(),
            (RADIO_CHANNEL)packet->getChannelId(), 0);
    //rssi and channel number are not reported
}

void MosaicEventScheduler::processShutDown() {
    EV_DEBUG << "MosaicEventScheduler received shut down command" << endl;
    cMessage *finMessage = new cMessage("MosaicFinishCmd", 22);
    finMessage->setTimestamp(m_currentMaxSimTime);
    finMessage->setArrivalTime(m_currentMaxSimTime);
    finMessage->setArrival(mgmt->getId(), -1);
    putBackEvent(finMessage);
}

void MosaicEventScheduler::processUpdateNode() {
    CSC_update_node_return update_node_message;
    m_ambassadorFederateChannel->readUpdateNode(update_node_message);

    simtime_t time(update_node_message.time, SimTimeUnit::SIMTIME_NS);
    const unsigned int numNodes = update_node_message.properties.size();

    EV_DEBUG << "MosaicEventScheduler received UPDATE_NODE command: " << time.str()
                     << " for " << numNodes << " nodes" << endl;

    MosaicMobilityCmd *cmdMessage;
    if (update_node_message.type == UPDATE_ADD_VEHICLE) {
        cmdMessage = processUpdateNodeCommand(numNodes, update_node_message, MOBILITY_CMD_ADD_NODES);
    } else if (update_node_message.type == UPDATE_ADD_RSU) {
        cmdMessage = processUpdateNodeCommand(numNodes, update_node_message, MOBILITY_CMD_ADD_RSU_NODES);
    } else if (update_node_message.type == UPDATE_MOVE_NODE) {
        cmdMessage = processUpdateNodeCommand(numNodes, update_node_message, MOBILITY_CMD_MOVE_NODES);
    } else if (update_node_message.type == UPDATE_REMOVE_NODE) {
        cmdMessage = processUpdateNodeCommand(numNodes, update_node_message, MOBILITY_CMD_REMOVE_NODES, false);
    }

    cmdMessage->setTimestamp(time);
    cmdMessage->setArrivalTime(time);
    cmdMessage->setArrival(mgmt->getId(), -1);

    putBackEvent(cmdMessage);

    EV_DEBUG << "MosaicEventScheduler finished processing of command" << endl;
    m_ambassadorFederateChannel->writeCommand(CMD_SUCCESS);
}

MosaicMobilityCmd* MosaicEventScheduler::processUpdateNodeCommand (const unsigned int numNodes,
        CSC_update_node_return& update_node_message,
        MobilityCommandType cmd_type,
        const bool newPosition) {
    auto cmdMessage = new MosaicMobilityCmd("MosaicMobilityCmd");
    cmdMessage->setCmdType(cmd_type);
    cmdMessage->setNodeIdArraySize(numNodes);
    cmdMessage->setPositionArraySize(newPosition ? numNodes : 0);

    for (std::vector<CSC_node_data>::iterator it = update_node_message.properties.begin();
            it != update_node_message.properties.end(); ++it) {
        EV_DEBUG << "MosaicEventScheduler " << cmd_type << ": " << it->id
                << " at position " << it->x << "," << it->y << std::endl;
        const int i = it - update_node_message.properties.begin();
        cmdMessage->setNodeId(i, it->id);
        if (newPosition) {
            inet::Coord coord;
            coord.x = it->x;
            coord.y = it->y;
            coord.z = 0;
            cmdMessage->setPosition(i, coord);
        }
    }
    return cmdMessage;
}

void MosaicEventScheduler::processMsgSend() {
    CSC_send_message send_message;
    m_ambassadorFederateChannel->readSendMessage(send_message);
    simtime_t   time(send_message.time, SimTimeUnit::SIMTIME_NS);

    EV_DEBUG << "MosaicEventScheduler.processMsgSend() received time: " << time.str() << endl;

    auto *comMessage = new MosaicCommunicationCmd("MosaicCommunicationCmd");
    comMessage->setCmdType(COMMUNICATION_CMD_SEND_MESSAGE);
    comMessage->setTimestamp(time);
    comMessage->setArrivalTime(time);
    comMessage->setArrival(mgmt->getId(), -1);
    comMessage->setNodeId(send_message.node_id);
    comMessage->setChannelId(send_message.channel_id);
    comMessage->setMsgId(send_message.message_id);
    comMessage->setLength((inet::B) send_message.length);

    //For now only Topo-unicast
    comMessage->setDestAddr(inet::Ipv4Address(send_message.topo_address.ip_address));
    comMessage->setTtl(send_message.topo_address.ttl);

    EV_DEBUG << "MosaicEventScheduler SEND_MESSAGE: from=" << comMessage->getNodeId()
                     << ", destAddress: " << comMessage->getDestAddr() << ", id=" << comMessage->getMsgId() << ", prot=udp" << std::endl;

    putBackEvent(comMessage);

    m_ambassadorFederateChannel->writeCommand(CMD_SUCCESS);
    EV_DEBUG << "MosaicEventScheduler finished processing of command" << std::endl;
}

void MosaicEventScheduler::processConfRadio() {
    CSC_config_message config_message;
    m_ambassadorFederateChannel->readConfigurationMessage(config_message);
    simtime_t   time(config_message.time, SimTimeUnit::SIMTIME_NS);

    EV_DEBUG << "MosaicEventScheduler received time: " << time.str() << endl;

    auto *confMessage = new MosaicConfigurationCmd("MosaicConfigurationCmd");
    confMessage->setCmdType(CONFIGURATION_CMD_CONF_RADIO);
    confMessage->setTimestamp(time);
    confMessage->setArrivalTime(time);
    confMessage->setArrival(mgmt->getId(), -1);
    confMessage->setMsgId(config_message.msg_id);
    confMessage->setNodeId(config_message.node_id);
    if (config_message.num_radios == SINGLE_RADIO) {
        confMessage->setNumRadios(1);
    } else if (config_message.num_radios == DUAL_RADIO) {
        confMessage->setNumRadios(2);
    } else if (config_message.num_radios == NO_RADIO){
        confMessage->setNumRadios(0);
    }
    if (config_message.num_radios == SINGLE_RADIO || config_message.num_radios == DUAL_RADIO) {
        confMessage->setTurnedOn0(config_message.primary_radio.turnedOn);
        confMessage->setIp0(inet::Ipv4Address(config_message.primary_radio.ip_address));
        confMessage->setSubnet0(inet::Ipv4Address(config_message.primary_radio.subnet));
        confMessage->setPower0(config_message.primary_radio.tx_power);
        if (config_message.primary_radio.channelmode == SINGLE_CHANNEL) {
            confMessage->setNumchannels0(1);
            confMessage->setChannel00(config_message.primary_radio.primary_channel);
        } else if (config_message.primary_radio.channelmode == DUAL_CHANNEL) {
            confMessage->setNumchannels0(2);
            confMessage->setChannel00(config_message.primary_radio.primary_channel);
            confMessage->setChannel01(config_message.primary_radio.secondary_channel);
        }
    }
    if (config_message.num_radios == DUAL_RADIO) {
        confMessage->setTurnedOn1(config_message.secondary_radio.turnedOn);
        confMessage->setIp1(inet::Ipv4Address(config_message.secondary_radio.ip_address));
        confMessage->setSubnet1(inet::Ipv4Address(config_message.secondary_radio.subnet));
        confMessage->setPower1(config_message.secondary_radio.tx_power);
        if (config_message.secondary_radio.channelmode == SINGLE_CHANNEL) {
            confMessage->setNumchannels1(1);
            confMessage->setChannel10(config_message.secondary_radio.primary_channel);
        } else if (config_message.secondary_radio.channelmode == DUAL_CHANNEL) {
            confMessage->setNumchannels1(2);
            confMessage->setChannel10(config_message.secondary_radio.primary_channel);
            confMessage->setChannel11(config_message.secondary_radio.secondary_channel);
        }
    }
    EV_DEBUG << "MosaicEventScheduler CONF_RADIO: at " << time
            << " from=" << confMessage->getNodeId() << ", id=" << confMessage->getMsgId() << std::endl;

    putBackEvent(confMessage);

    EV_DEBUG << "MosaicEventScheduler finished processing of command" << std::endl;
    m_ambassadorFederateChannel->writeCommand(CMD_SUCCESS);
}

void MosaicEventScheduler::processAdvanceTime() {
    const int64_t newMaxTime = m_ambassadorFederateChannel->readTimeMessage();
    m_currentMaxSimTime = SimTime(newMaxTime, SimTimeUnit::SIMTIME_NS);
    m_timeAdvancing = true;
    EV_DEBUG << "MosaicEventScheduler ADVANCE_TIME: " << m_currentMaxSimTime << std::endl;
}

void MosaicEventScheduler::receiveInteractions() {
    CMD command;
    EV_DEBUG << "MosaicEventScheduler wait new command" << std::endl;
    command = m_ambassadorFederateChannel->readCommand();
    EV_DEBUG << "MosaicEventScheduler received command: " << command << std::endl;

    switch (command) {
        case CMD_SHUT_DOWN: processShutDown(); break;
        case CMD_UPDATE_NODE: processUpdateNode(); break;
        case CMD_MSG_SEND: processMsgSend(); break;
        case CMD_CONF_RADIO: processConfRadio(); break;
        case CMD_ADVANCE_TIME: processAdvanceTime(); break;
        default: {
            m_ambassadorFederateChannel->writeCommand(CMD_END);
            EV_DEBUG << "MosaicEventScheduler Received unknown command from ambassador, ending" << std::endl;
            m_timeAdvancing = true;
            cRuntimeError("MosaicEventScheduler FAILURE (received unknown command %d)", command);
        }
    }
}

} // namespace omnetpp_federate
