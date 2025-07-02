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

#include "MosaicScenarioManager.h"

#include <sstream>

#include "inet/common/Ptr.h"
#include "inet/common/geometry/common/Coord.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/ipv4/Ipv4InterfaceData.h"

#include "msg/MosaicAppPacket_m.h"
#include "msg/MosaicCommunicationCmd_m.h"
#include "msg/MosaicConfigurationCmd_m.h"
#include "msg/MosaicMobilityCmd_m.h"
#include "node/MosaicMobility.h"
#include "node/MosaicProxyApp.h"

namespace omnetpp_federate {
using namespace omnetpp;

Define_Module(MosaicScenarioManager);

/**
 * Initialize members and setup connection to Ambassador.
 */
void MosaicScenarioManager::initialize() {
  vehModuleType = par("vehModuleType").stdstringValue();
  vehModuleName = par("vehModuleName").stdstringValue();
  rsuModuleType = par("rsuModuleType").stdstringValue();
  rsuModuleName = par("rsuModuleName").stdstringValue();
  moduleDisplayString = par("moduleDisplayString").stdstringValue();

  sched = dynamic_cast<MosaicEventScheduler *>(
      cSimulation::getActiveSimulation()->getScheduler());
  if (nullptr == sched) {
    EV_ERROR << "MosaicEventScheduler has to be used as a scheduler!"
             << std::endl;
  }
  sched->setMgmtModule(this);

  EV_INFO << "MosaicScenarioManager initialized: vehModuleType: " << std::endl;
}

/**
 * Process duly received and scheduled commands from Mosaic on the on hand and
 * received v2x messages from omnet internal application layer on the other
 * hand.
 *
 * @param msg
 * 		(event) message
 */
void MosaicScenarioManager::handleMessage(cMessage *msg) {
  int nodeId;
  inet::Coord position;

  EV << "handleMessage() - name: " << msg->getName()
     << ", kind: " << msg->getKind() << std::endl;

  if (strcmp(msg->getName(), "MosaicMobilityCmd") == 0) {
    MosaicMobilityCmd *cmd = check_and_cast<MosaicMobilityCmd *>(msg);
    if (cmd->getCmdType() == MOBILITY_CMD_ADD_NODES) {
      for (unsigned int i = 0; i < cmd->getNodeIdArraySize(); i++) {
        nodeId = cmd->getNodeId(i);
        position = cmd->getPosition(i);
        EV_DEBUG << "MosaicScenarioManager ADD_NODES: " << nodeId
                 << " at position " << position.x << "," << position.y
                 << std::endl;
        addNode(nodeId, position);
      }
    } else if (cmd->getCmdType() == MOBILITY_CMD_ADD_RSU_NODES) {
      for (unsigned int i = 0; i < cmd->getNodeIdArraySize(); i++) {
        nodeId = cmd->getNodeId(i);
        position = cmd->getPosition(i);
        EV_DEBUG << "MosaicScenarioManager ADD_RSU_NODES: " << nodeId
                 << " at position " << position.x << "," << position.y
                 << std::endl;
        addRsuNode(nodeId, position);
      }
    } else if (cmd->getCmdType() == MOBILITY_CMD_MOVE_NODES) {
      for (unsigned int i = 0; i < cmd->getNodeIdArraySize(); i++) {
        nodeId = cmd->getNodeId(i);
        position = cmd->getPosition(i);
        EV_DEBUG << "MosaicScenarioManager MOVE_NODES: " << nodeId
                 << " to position " << position.x << "," << position.y
                 << std::endl;
        moveNode(nodeId, position);
      }
    } else if (cmd->getCmdType() == MOBILITY_CMD_REMOVE_NODES) {
      for (unsigned int i = 0; i < cmd->getNodeIdArraySize(); i++) {
        nodeId = cmd->getNodeId(i);
        EV_DEBUG << "MosaicScenarioManager REMOVE_NODES: " << nodeId
                 << std::endl;
        removeNode(nodeId);
      }
    }
  } else if (strcmp(msg->getName(), "MosaicCommunicationCmd") == 0) {
    MosaicCommunicationCmd *cmd = check_and_cast<MosaicCommunicationCmd *>(msg);
    if (cmd->getCmdType() == COMMUNICATION_CMD_SEND_MESSAGE) {
      EV_DEBUG << "MosaicScenarioManager SEND_MESSAGE" << std::endl;
      sendV2xMessage(msg);
    }
  } else if (strcmp(msg->getName(), "MosaicConfigurationCmd") == 0) {
    MosaicConfigurationCmd *cmd = check_and_cast<MosaicConfigurationCmd *>(msg);
    if (cmd->getCmdType() == CONFIGURATION_CMD_CONF_RADIO) {
      EV_DEBUG << "MosaicScenarioManager CONF_RADIO" << std::endl;
      configureRadio(msg);
    }
  } else if (strcmp(msg->getName(), "MosaicFinishCmd") == 0) {
    EV_DEBUG << "MosaicScenarioManager FINISH" << std::endl;
    delete msg; // Delete msg because finish() terminates the program.
    finish();
  } else {
    // Send message back to mosaic, since it was received from mosaic udp app
    // (mosaic tcp app is not activated up to now, as no real use case exists
    // for simulation)
    receiveV2xMessage(msg);
  }
  delete msg;
}

/**
 * Finish simulation.
 */
void MosaicScenarioManager::finish() {
  static bool once = false;
  if (!once) {
    // toggle the once flag
    once = true;

    // Tidy up and finish simulation
    while (nodes.begin() != nodes.end()) {
      removeNode(nodes.begin()->first);
    }
    sched->endRun();
    EV << "MosaicScenarioManager simulation ended" << std::endl;
    endSimulation();
  }
}

cModule *MosaicScenarioManager::getManagedModule(int nodeId) {
  if (nodes.find(nodeId) == nodes.end()) {
    return nullptr;
  }
  return nodes[nodeId];
}

void MosaicScenarioManager::addModule(int nodeId, std::string type,
                                      std::string name,
                                      std::string displayString) {
  if (nodes.find(nodeId) != nodes.end()) {
    error("Tried adding duplicate module");
  }

  cModule *parentmod = getParentModule();
  if (!parentmod) {
    error("Parent module not found");
  }

  cModuleType *nodeType = cModuleType::get(type.c_str());
  if (!nodeType) {
    error("Module type \"%s\" not found", type.c_str());
  }

  unsigned int nodeVectorIndex = nodeId;
  cModule *mod = nodeType->create(name.c_str(), parentmod, nodeVectorIndex,
                                  nodeVectorIndex);
  mod->finalizeParameters();
  mod->getDisplayString().parse(displayString.c_str());
  mod->buildInside();
  mod->scheduleStart(simTime());
  nodes[nodeId] = mod;
}

void MosaicScenarioManager::addNode(int nodeId, inet::Coord &position) {
  cModule *newNode = getManagedModule(nodeId);
  if (newNode) {
    error("Tried adding duplicate node (vehicle) %d", nodeId);
  }

  addModule(nodeId, vehModuleType, vehModuleName, moduleDisplayString);
  newNode = getManagedModule(nodeId);

  MosaicProxyApp *app;
  MosaicMobility *mobility;

  cModule *submod = newNode->getSubmodule("proxyApp", 0);
  if ((app = dynamic_cast<MosaicProxyApp *>(submod))) {
    this->setGateSize("mosaicProxyOut", this->gateSize("mosaicProxyOut") + 1);
    this->gate("mosaicProxyOut", nodeId)->connectTo(app->gate("fedIn"));
    this->setGateSize("mosaicProxyIn", this->gateSize("mosaicProxyIn") + 1);
    app->gate("fedOut")->connectTo(this->gate("mosaicProxyIn", nodeId));
    app->setExternalId(nodeId);
  }

  submod = newNode->getSubmodule("mobility");
  if ((mobility = dynamic_cast<MosaicMobility *>(submod))) {
    // Initialize mosaicmobility module (external id and initial position)
    mobility->setExternalId(nodeId);
    mobility->setNextPosition(position);
  }

  newNode->callInitialize();
  EV << "MosaicScenarioManager added vehicle " << nodeId << " at position "
     << position.x << "," << position.y << " at time " << simTime()
     << std::endl;
}

void MosaicScenarioManager::addRsuNode(int nodeId, inet::Coord &position) {
  cModule *newNode = getManagedModule(nodeId);
  if (newNode) {
    error("Tried adding duplicate node (rsu) %d", nodeId);
  }

  addModule(nodeId, rsuModuleType, rsuModuleName, moduleDisplayString);
  newNode = getManagedModule(nodeId);

  MosaicProxyApp *app;
  MosaicMobility *mobility;

  cModule *submod = newNode->getSubmodule("proxyApp", 0);
  if ((app = dynamic_cast<MosaicProxyApp *>(submod))) {
    this->setGateSize("mosaicProxyOut", this->gateSize("mosaicProxyOut") + 1);
    this->gate("mosaicProxyOut", nodeId)->connectTo(app->gate("fedIn"));
    this->setGateSize("mosaicProxyIn", this->gateSize("mosaicProxyIn") + 1);
    app->gate("fedOut")->connectTo(this->gate("mosaicProxyIn", nodeId));
    app->setExternalId(nodeId);
  }

  submod = newNode->getSubmodule("mobility");
  if ((mobility = dynamic_cast<MosaicMobility *>(submod))) {
    mobility->setExternalId(nodeId);
    mobility->setNextPosition(position);
  }

  newNode->callInitialize();
  EV_DEBUG << "MosaicScenarioManager added rsu " << nodeId << " at position "
           << position.x << "," << position.y << " at time " << simTime()
           << std::endl;
}

void MosaicScenarioManager::moveNode(int nodeId, inet::Coord &position) {
  cModule *mod = getManagedModule(nodeId);
  if (!mod) {
    EV_WARN << "WARNING: Node " << nodeId << " not mapped" << std::endl;
  } else {
    for (cModule::SubmoduleIterator iter(mod); !iter.end(); iter++) {
      cModule *submod = *iter;
      MosaicMobility *mm = dynamic_cast<MosaicMobility *>(submod);
      if (!mm) {
        continue;
      }
      mm->setNextPosition(position);
    }
    EV_DEBUG << "MosaicScenarioManager moved vehicle " << nodeId
             << " to position " << position.x << "," << position.y
             << " at time " << simTime() << std::endl;
  }
}

void MosaicScenarioManager::removeNode(int nodeId) {
  cModule *mod = getManagedModule(nodeId);
  if (!mod) {
    error("No node with id %d found", nodeId);
  } else {
    nodes.erase(nodeId);
    mod->callFinish();
    // we have to delete the two radios before we delete the node, since the
    // MediumLimitCache tries to determine the minimal constrained area from the
    // nonexistent mobility while deleting the radios.
    cModule *wlan0Module = mod->getSubmodule("wlan0");
    if (wlan0Module != nullptr) {
      wlan0Module->deleteModule();
    }
    cModule *wlan1Module = mod->getSubmodule("wlan1");
    if (wlan1Module != nullptr) {
      wlan1Module->deleteModule();
    }
    mod->deleteModule();

    EV << "MosaicScenarioManager removed node " << nodeId << " at time "
       << simTime() << std::endl;
  }
}

void MosaicScenarioManager::sendV2xMessage(cMessage *msg) {
  auto *cmd = check_and_cast<MosaicCommunicationCmd *>(msg);
  int nodeId = cmd->getNodeId();
  cModule *mod = getManagedModule(nodeId);
  if (!mod) {
    EV << "WARNING: Node " << nodeId << " not mapped" << std::endl;
  } else {
    if (cmd->getTtl() != 1) {
      EV << "WARNING: up to now only singlehop-broadcast supported"
         << std::endl;
    } else {
      int msgId = cmd->getMsgId();
      char packetName[32];
      sprintf(packetName, "V2xPacket-%d", msgId);
      auto *packet = new MosaicAppPacket(packetName);
      packet->setNodeId(nodeId);
      packet->setMsgId(msgId);
      packet->setDestAddr(cmd->getDestAddr());
      packet->setChannelId(cmd->getChannelId());

      auto chunk = inet::makeShared<inet::ByteCountChunk>(cmd->getLength());
      packet->insertAtFront(chunk);

      send(packet->dup(), "mosaicProxyOut", nodeId);
      EV << "MosaicScenarioManager send udp message " << msgId << " from node "
         << nodeId << " at time " << simTime() << std::endl;
    }
  }
}

void MosaicScenarioManager::receiveV2xMessage(cMessage *msg) {
  auto *packet = check_and_cast<MosaicAppPacket *>(msg);
  EV_DEBUG << "MosaicScenarioManager receive v2x message " << packet->getMsgId()
           << " on node " << packet->getNodeId() << " at time " << simTime()
           << " on channel " << packet->getChannelId() << std::endl;
  sched->reportReceivedV2xMessage(msg);
}

void MosaicScenarioManager::configureRadio(cMessage *msg) {
  auto *cmd = check_and_cast<MosaicConfigurationCmd *>(msg);
  if (cmd == nullptr) {
    return;
  }
  int nodeId = cmd->getNodeId();
  cModule *mod = getManagedModule(nodeId);
  if (!mod) {
    EV_WARN << "WARNING: Node " << nodeId << " not mapped" << std::endl;
  } else {
    send(msg->dup(), "mosaicProxyOut", nodeId);
  }
}

} // namespace omnetpp_federate
