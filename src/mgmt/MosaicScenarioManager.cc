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

// Simu5G includes for debugging binder registration
#include "common/binder/Binder.h"
#include "common/LteCommon.h"

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
  nodeBModuleType = par("nodeBModuleType").stdstringValue();
  nodeBModuleName = par("nodeBModuleName").stdstringValue();
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

  const std::string_view msgName = msg->getName();
  if (msgName == "MosaicMobilityCmd") {
    auto cmd = check_and_cast<MosaicMobilityCmd *>(msg);
    EV << "MosaicScenarioManager received command: " << cmd->getCmdType()
       << std::endl;
    if (cmd->getCmdType() == MOBILITY_CMD_ADD_RADIO_VEH_NODE) {
      for (unsigned int i = 0; i < cmd->getNodeIdArraySize(); i++) {
        nodeId = cmd->getNodeId(i);
        position = cmd->getPosition(i);
        EV_DEBUG << "MosaicScenarioManager ADD_NODES: " << nodeId
                 << " at position " << position.x << "," << position.y
                 << std::endl;
        addNode(nodeId, position);
      }
    } else if (cmd->getCmdType() == MOBILITY_CMD_ADD_RADIO_RSU_NODE) {
      for (unsigned int i = 0; i < cmd->getNodeIdArraySize(); i++) {
        nodeId = cmd->getNodeId(i);
        position = cmd->getPosition(i);
        EV_DEBUG << "MosaicScenarioManager ADD_RSU_NODES: " << nodeId
                 << " at position " << position.x << "," << position.y
                 << std::endl;
        addRsuNode(nodeId, position);
      }
    } else if (cmd->getCmdType() == MOBILITY_CMD_ADD_NODE_B) {
      for (unsigned int i = 0; i < cmd->getNodeIdArraySize(); i++) {
        nodeId = cmd->getNodeId(i);
        position = cmd->getPosition(i);
        EV << "MosaicScenarioManager ADD_NODE_B: " << nodeId << " at position "
           << position.x << "," << position.y << std::endl;
        addNodeB(nodeId, position);
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
    } else if (cmd->getCmdType() == MOBILITY_CMD_REMOVE_NODE) {
      for (unsigned int i = 0; i < cmd->getNodeIdArraySize(); i++) {
        nodeId = cmd->getNodeId(i);
        EV_DEBUG << "MosaicScenarioManager REMOVE_NODES: " << nodeId
                 << std::endl;
        removeNode(nodeId);
      }
    }
  } else if (msgName == "MosaicCommunicationCmd") {
    auto *cmd = check_and_cast<MosaicCommunicationCmd *>(msg);
    if (cmd->getCmdType() == COMMUNICATION_CMD_SEND_WIFI_MESSAGE) {
      EV_DEBUG << "MosaicScenarioManager SEND_WIFI_MESSAGE" << std::endl;
      sendV2xMessage(msg);
    } else if (cmd->getCmdType() == COMMUNICATION_CMD_SEND_CELL_MESSAGE) {
      EV_DEBUG << "MosaicScenarioManager SEND_CELL_MESSAGE" << std::endl;
      sendV2xMessage(msg);
    }
  } else if (msgName == "MosaicWifiConfigurationCmd") {
    auto *cmd = check_and_cast<MosaicWifiConfigurationCmd *>(msg);
    EV_DEBUG << "MosaicScenarioManager WIFI_CONF_RADIO" << std::endl;
    configureRadio(msg);
  } else if (msgName == "MosaicCellularConfigurationCmd") {
    auto *cmd = check_and_cast<MosaicCellularConfigurationCmd *>(msg);
    EV_DEBUG << "MosaicScenarioManager CELLULAR_CONF_RADIO" << std::endl;
    configureRadio(msg);
  } else if (msgName == "MosaicFinishCmd") {
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
  // Search for the node with the given nodeId in the nodes map
  auto it = nodes.find(nodeId);
  // Return the module pointer if found, nullptr otherwise
  return (it != nodes.end()) ? it->second : nullptr;
}

void MosaicScenarioManager::addModule(int nodeId, const std::string &type,
                                      const std::string &name,
                                      const std::string &displayString) {
  // Check if the nodeId already exists in the nodes map
  // If it does, throw an error to prevent duplicate module creation
  const auto [it, inserted] = nodes.try_emplace(nodeId, nullptr);
  if (!inserted) {
    error("Tried adding duplicate module");
  }

  auto *parentmod = getParentModule();
  if (!parentmod) {
    error("Parent module not found");
  }

  auto *nodeType = cModuleType::get(type.c_str());
  if (!nodeType) {
    error("Module type \"%s\" not found", type.c_str());
  }

  unsigned int nodeVectorIndex;

  // OMNeT++ 6.x: Ensure submodule vector exists before creating modules
  if (!parentmod->hasSubmoduleVector(name.c_str())) {
    // Add submodule vector with initial size to accommodate this node
    parentmod->addSubmoduleVector(name.c_str(), 1);
    EV << "Added submodule vector \"" << name << "\" with size " << (1)
       << std::endl;
    nodeVectorIndex = 0; // Reset index to 0 for new vector
  } else {
    nodeVectorIndex = parentmod->getSubmoduleVectorSize(name.c_str());
    // Expand existing vector if needed
    parentmod->setSubmoduleVectorSize(name.c_str(), nodeVectorIndex + 1);
    EV << "Expanded submodule vector \"" << name << "\" to size "
       << (nodeVectorIndex + 1) << std::endl;
  }
  EV << "Creating module of type \"" << type << "\" with name \"" << name
     << "\" at index " << nodeVectorIndex << " in parent module \""
     << parentmod->getFullName() << "\"" << std::endl;
  auto *mod = nodeType->create(name.c_str(), parentmod, nodeVectorIndex);

  // For gNodeB modules, we need to create a custom instance that uses
  // MosaicMobility Since the gNodeB hardcodes StationaryMobility, we'll handle
  // this in addNodeB method

  mod->finalizeParameters();
  mod->getDisplayString().parse(displayString.c_str());
  mod->buildInside();

  // For gNodeB, don't schedule start yet - we need to replace mobility first
  if (type != nodeBModuleType) {

    mod->scheduleStart(simTime());
  } else {
    EV << mod->par("nodeType") << " is a gNodeB, not scheduling start yet"
       << std::endl;
  }

  it->second = mod;
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

  auto *submod = newNode->getSubmodule("proxyApp");
  if ((app = dynamic_cast<MosaicProxyApp *>(submod))) {
    // Ensure gate vector is large enough for nodeId
    if (this->gateSize("mosaicProxyOut") <= nodeId) {
      this->setGateSize("mosaicProxyOut", nodeId + 1);
    }
    if (this->gateSize("mosaicProxyIn") <= nodeId) {
      this->setGateSize("mosaicProxyIn", nodeId + 1);
    }
    this->gate("mosaicProxyOut", nodeId)->connectTo(app->gate("fedIn"));
    app->gate("fedOut")->connectTo(this->gate("mosaicProxyIn", nodeId));
    app->setExternalId(nodeId);
  }

  submod = newNode->getSubmodule("mobility");
  if ((mobility = dynamic_cast<MosaicMobility *>(submod))) {
    // Initialize mosaicmobility module (external id and initial position)
    mobility->setExternalId(nodeId);
    mobility->setNextPosition(position);
  }

  // Configure cellular parameters if cellular NIC exists
  auto *cellularModule = newNode->getSubmodule("cellularNic");
  // if (cellularModule != nullptr) {
  //   // Set unique cellular identifiers for Simu5G registration
  //   newNode->par("macNodeId") = nodeId + 1000; // Ensure unique MAC node ID
  //   newNode->par("masterId") = 1;  // Connect to first gNodeB (master)
  //   newNode->par("macCellId") = 1; // Default cell ID

  //   EV << "Configured cellular parameters for vehicle " << nodeId
  //      << " macNodeId: " << (nodeId + 1000) << std::endl;

  //   // Check if IP2Nic submodule exists
  //   auto *ip2nicModule = cellularModule->getSubmodule("ip2nic");
  //   if (ip2nicModule != nullptr) {
  //     EV << "Vehicle " << nodeId << " has IP2Nic module for binder
  //     registration"
  //        << std::endl;
  //   } else {
  //     EV << "WARNING: Vehicle " << nodeId
  //        << " cellular NIC has no IP2Nic module!" << std::endl;
  //   }
  // } else {
  //   EV << "Vehicle " << nodeId << " has no cellular NIC module" << std::endl;
  // }

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

  auto *submod = newNode->getSubmodule("proxyApp");

  if ((app = dynamic_cast<MosaicProxyApp *>(submod))) {
    // Initialize mosaicproxyapp module (external id and connection to fed)
    EV << "MosaicScenarioManager adding rsu node " << nodeId << " with proxyApp"
       << std::endl;
    // Ensure gate vector is large enough for nodeId
    if (this->gateSize("mosaicProxyOut") <= nodeId) {
      this->setGateSize("mosaicProxyOut", nodeId + 1);
    }
    if (this->gateSize("mosaicProxyIn") <= nodeId) {
      this->setGateSize("mosaicProxyIn", nodeId + 1);
    }
    this->gate("mosaicProxyOut", nodeId)->connectTo(app->gate("fedIn"));
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

void MosaicScenarioManager::addNodeB(int nodeId, inet::Coord &position) {
  cModule *newNode = getManagedModule(nodeId);
  if (newNode) {
    error("Tried adding duplicate node (nodeB) %d", nodeId);
  }

  addModule(nodeId, nodeBModuleType, nodeBModuleName, moduleDisplayString);
  newNode = getManagedModule(nodeId);

  // Set display string with coordinates so ChannelAccess can find them as
  // fallback
  std::string displayStr = moduleDisplayString +
                           ";p=" + std::to_string((int)position.x) + "," +
                           std::to_string((int)position.y);
  newNode->getDisplayString().parse(displayStr.c_str());

  // Replace the hardcoded StationaryMobility with MosaicMobility
  auto *oldMobility = newNode->getSubmodule("mobility");
  if (oldMobility) {
    // Delete the existing StationaryMobility module
    oldMobility->deleteModule();

    // Create a new MosaicMobility module
    auto *mobilityType =
        cModuleType::get("omnetpp_federate.node.MosaicMobility");
    if (mobilityType) {
      auto *newMobility = mobilityType->create("mobility", newNode);
      newMobility->finalizeParameters();
      newMobility->buildInside();

      // Set position and ID after building (parameters are now accessible)
      auto *mobility = dynamic_cast<MosaicMobility *>(newMobility);
      if (mobility) {
        mobility->setExternalId(nodeId);
        mobility->setNextPosition(position); // This sets the position properly

        EV << "MosaicMobility created for nodeB " << nodeId << " at position "
           << position.x << "," << position.y << std::endl;
      }
    }
  }
  // Add initial macCellId
  // TODO: This get's overwritten if using dynamicAssociationMode (maybe set in
  // ned)
  newNode->par("macCellId") = nodeId + 1;
  newNode->par("macNodeId") = nodeId + 1;

  // Now schedule start for the gNodeB module after mobility is ready
  newNode->scheduleStart(simTime());
  newNode->callInitialize();
  // Establish PPP connection to UPF for cellular traffic routing

  cModule *upf = getParentModule()->getSubmodule("upf");
  if (upf && newNode) {
    // Get the next available index for UPF pppg gates
    int upfPppIndex = upf->gateSize("pppg$o");

    // Expand UPF gate vector to accommodate new connection
    upf->setGateSize("pppg", upfPppIndex + 1);

    cGate *gnbPppOut = newNode->gate("ppp$o");
    cGate *gnbPppIn = newNode->gate("ppp$i");
    cGate *upfPppOut = upf->gate("pppg$o", upfPppIndex);
    cGate *upfPppIn = upf->gate("pppg$i", upfPppIndex);

    // Create Ethernet channel for PPP connection (similar to Eth10G in
    // examples)
    cChannelType *channelType = cChannelType::get("inet.node.ethernet.Eth10G");
    if (channelType) {
      cChannel *channel1 = channelType->create("gNodeB-to-UPF");
      cChannel *channel2 = channelType->create("UPF-to-gNodeB");

      // Connect with proper channels: gNodeB.ppp <--> Eth10G <--> UPF.pppg[i]
      gnbPppOut->connectTo(upfPppIn, channel1);
      upfPppOut->connectTo(gnbPppIn, channel2);

      EV << "Connected gNodeB " << nodeId << " to UPF via pppg[" << upfPppIndex
         << "] with Eth10G channels for 5G core network routing" << std::endl;
    } else {
      EV_ERROR << "Failed to find Eth10G channel type for gNodeB-UPF connection"
               << std::endl;
    }
  }

  EV << "MosaicScenarioManager added nodeB " << nodeId << " at position "
     << position.x << "," << position.y << " at time " << simTime()
     << std::endl;
}

void MosaicScenarioManager::moveNode(int nodeId, inet::Coord &position) {
  cModule *mod = getManagedModule(nodeId);
  if (mod == nullptr) {
    EV_WARN << "WARNING: Node " << nodeId << " not mapped" << std::endl;
  } else {
    for (cModule::SubmoduleIterator iter(mod); !iter.end(); iter++) {
      auto *submod = *iter;
      auto *mm = dynamic_cast<MosaicMobility *>(submod);
      if (mm == nullptr) {
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
  if (mod == nullptr) {
    error("No node with id %d found", nodeId);
  } else {
    EV << "MosaicScenarioManager removing node " << nodeId
       << " (OMNeT++ ID: " << mod->getId() << ") at time " << simTime()
       << std::endl;

    nodes.erase(nodeId);
    // we have to delete the two radios before we delete the node, since the
    // MediumLimitCache tries to determine the minimal constrained area from the
    // nonexistent mobility while deleting the radios.
    auto *wlan0Module = mod->getSubmodule("wlan0");
    if (wlan0Module != nullptr) {
      wlan0Module->deleteModule();
    }
    auto *wlan1Module = mod->getSubmodule("wlan1");
    if (wlan1Module != nullptr) {
      wlan1Module->deleteModule();
    }

    mod->callFinish();
    mod->deleteModule();

    EV << "MosaicScenarioManager removed node " << nodeId << " at time "
       << simTime() << std::endl;
  }
}

void MosaicScenarioManager::sendV2xMessage(cMessage *msg) {
  auto *cmd = check_and_cast<MosaicCommunicationCmd *>(msg);
  int nodeId = cmd->getNodeId();
  cModule *mod = getManagedModule(nodeId);
  if (mod == nullptr) {
    EV << "WARNING: Node " << nodeId << " not mapped" << std::endl;
  } else {
    // Check if WiFi or cellular based on command type
    bool isCellular =
        (cmd->getCmdType() == COMMUNICATION_CMD_SEND_CELL_MESSAGE);

    if (!isCellular && cmd->getTtl() != 1) {
      EV << "WARNING: up to now only singlehop-broadcast supported for WiFi"
         << std::endl;
    } else {
      const auto msgId = cmd->getMsgId();
      const auto packetName = "V2xPacket" + std::to_string(msgId);
      auto *packet = new MosaicAppPacket(packetName.c_str());
      packet->setNodeId(nodeId);
      packet->setMsgId(msgId);
      packet->setDestAddr(cmd->getDestAddr());

      // Set channel: -1 for cellular, actual channel for WiFi
      if (isCellular) {
        packet->setChannelId(-1); // Cellular indicator
      } else {
        packet->setChannelId(cmd->getChannelId()); // WiFi channel
      }

      const auto chunk =
          inet::makeShared<inet::ByteCountChunk>(cmd->getLength());
      packet->insertAtFront(chunk);

      send(packet, "mosaicProxyOut", nodeId);
      EV << "MosaicScenarioManager send " << (isCellular ? "cellular" : "wifi")
         << " message " << msgId << " from node " << nodeId << " at time "
         << simTime() << std::endl;
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
  if (mod == nullptr) {
    EV_WARN << "WARNING: Node " << nodeId << " not mapped" << std::endl;
  } else {
    send(msg->dup(), "mosaicProxyOut", nodeId);
  }
}

} // namespace omnetpp_federate
