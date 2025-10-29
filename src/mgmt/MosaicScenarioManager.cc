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
#include "inet/networklayer/ipv4/Ipv4RoutingTable.h"
#include "inet/networklayer/configurator/ipv4/Ipv4NetworkConfigurator.h"

#include "msg/MosaicAppPacket_m.h"
#include "msg/MosaicCommunicationCmd_m.h"
#include "msg/MosaicConfigurationCmd_m.h"
#include "msg/MosaicMobilityCmd_m.h"
#include "node/MosaicMobility.h"
#include "node/MosaicProxyApp.h"

// Simu5G includes for debugging binder registration
#include "common/binder/Binder.h"
#include "common/LteCommon.h"
#include <inet/networklayer/contract/IInterfaceTable.h>

namespace omnetpp_federate
{
  using namespace omnetpp;

  Define_Module(MosaicScenarioManager);

  /**
   * Initialize members and setup connection to Ambassador.
   */
  void MosaicScenarioManager::initialize()
  {
    vehModuleType = par("vehModuleType").stdstringValue();
    vehModuleName = par("vehModuleName").stdstringValue();
    rsuModuleType = par("rsuModuleType").stdstringValue();
    rsuModuleName = par("rsuModuleName").stdstringValue();
    nodeBModuleType = par("nodeBModuleType").stdstringValue();
    nodeBModuleName = par("nodeBModuleName").stdstringValue();
    wiredModuleType = par("wiredModuleType").stdstringValue();
    wiredModuleName = par("wiredModuleName").stdstringValue();
    moduleDisplayString = par("moduleDisplayString").stdstringValue();

    sched = dynamic_cast<MosaicEventScheduler *>(
        cSimulation::getActiveSimulation()->getScheduler());
    if (nullptr == sched)
    {
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
  void MosaicScenarioManager::handleMessage(cMessage *msg)
  {
    int nodeId;
    inet::Coord position;

    EV << "handleMessage() - name: " << msg->getName()
       << ", kind: " << msg->getKind() << std::endl;

    const std::string_view msgName = msg->getName();
    if (msgName == "MosaicMobilityCmd")
    {
      auto cmd = check_and_cast<MosaicMobilityCmd *>(msg);
      EV << "MosaicScenarioManager received command: " << cmd->getCmdType()
         << std::endl;
      if ((cmd->getCmdType() != MOBILITY_CMD_ADD_NODE_B && cmd->getCmdType() != MOBILITY_CMD_ADD_WIRED_NODE) &&
          !nodeBRegistrationDone)
      {
        // First non-NodeB message after NodeB configuration - add all NodeBs to simulation
        if (!pendingNodeBs.empty())
        {
          EV << "Adding " << pendingNodeBs.size() << " configured NodeBs to simulation" << std::endl;
          auto numX2Apps = pendingNodeBs.size() - 1; // One X2App per peer NodeB

          for (const auto &config : pendingNodeBs)
          {
            // Get the nodeB
            cModule *nodeB = getManagedModule(config.nodeId);
            if (nodeB)
            {
              // Set the number of X2Apps
              nodeB->par("numX2Apps") = numX2Apps;
              // Finalize parameters for the nodeB
              nodeB->finalizeParameters();
            }
          }
          configureX2Connections();
          addAllNodeBsToSimulation();
        }

        nodeBRegistrationDone = true;
        
      }
      if (cmd->getCmdType() == MOBILITY_CMD_ADD_RADIO_VEH_NODE)
      {
        for (unsigned int i = 0; i < cmd->getNodeIdArraySize(); i++)
        {
          nodeId = cmd->getNodeId(i);
          position = cmd->getPosition(i);
          EV_DEBUG << "MosaicScenarioManager ADD_NODES: " << nodeId
             << " at position " << position.x << "," << position.y
             << std::endl;
          addNode(nodeId, position);
        }
      }
      else if (cmd->getCmdType() == MOBILITY_CMD_ADD_RADIO_RSU_NODE)
      {
        for (unsigned int i = 0; i < cmd->getNodeIdArraySize(); i++)
        {
          nodeId = cmd->getNodeId(i);
          position = cmd->getPosition(i);
          EV_DEBUG << "MosaicScenarioManager ADD_RSU_NODES: " << nodeId
             << " at position " << position.x << "," << position.y
             << std::endl;
          addRsuNode(nodeId, position);
        }
      }
      else if (cmd->getCmdType() == MOBILITY_CMD_ADD_WIRED_NODE)
      {
        for (unsigned int i = 0; i < cmd->getNodeIdArraySize(); i++)
        {
          nodeId = cmd->getNodeId(i);
          position = cmd->getPosition(i);
          EV_DEBUG << "MosaicScenarioManager ADD_WIRED_NODES: " << nodeId
             << " at position " << position.x << "," << position.y
             << std::endl;
          addWiredNode(nodeId, position);
        }
      }
      else if (cmd->getCmdType() == MOBILITY_CMD_ADD_NODE_B)
      {
        for (unsigned int i = 0; i < cmd->getNodeIdArraySize(); i++)
        {
          nodeId = cmd->getNodeId(i);
          position = cmd->getPosition(i);
          EV_DEBUG << "MosaicScenarioManager CONFIGURE_NODE_B: " << nodeId << " at position "
             << position.x << "," << position.y << " (configured but not added to simulation yet)" << std::endl;
          addNodeB(nodeId, position);
        }
      }
      else if (cmd->getCmdType() == MOBILITY_CMD_MOVE_NODES)
      {
        for (unsigned int i = 0; i < cmd->getNodeIdArraySize(); i++)
        {
          nodeId = cmd->getNodeId(i);
          position = cmd->getPosition(i);
          EV_DEBUG << "MosaicScenarioManager MOVE_NODES: " << nodeId
             << " to position " << position.x << "," << position.y
             << std::endl;
          moveNode(nodeId, position);
        }
      }
      else if (cmd->getCmdType() == MOBILITY_CMD_REMOVE_NODE)
      {
        for (unsigned int i = 0; i < cmd->getNodeIdArraySize(); i++)
        {
          nodeId = cmd->getNodeId(i);
          EV_DEBUG << "MosaicScenarioManager REMOVE_NODES: " << nodeId
                   << std::endl;
          removeNode(nodeId);
        }
      }
    }
    else if (msgName == "MosaicCommunicationCmd")
    {
      auto *cmd = check_and_cast<MosaicCommunicationCmd *>(msg);
      if (cmd->getCmdType() == COMMUNICATION_CMD_SEND_WIFI_MESSAGE)
      {
        EV_DEBUG << "MosaicScenarioManager SEND_WIFI_MESSAGE" << std::endl;
        sendV2xMessage(msg);
      }
      else if (cmd->getCmdType() == COMMUNICATION_CMD_SEND_CELL_MESSAGE)
      {
        EV_DEBUG << "MosaicScenarioManager SEND_CELL_MESSAGE" << std::endl;
        sendV2xMessage(msg);
      }
    }
    else if (msgName == "MosaicWifiConfigurationCmd")
    {
      auto *cmd = check_and_cast<MosaicWifiConfigurationCmd *>(msg);
      EV_DEBUG << "MosaicScenarioManager WIFI_CONF_RADIO" << std::endl;
      configureRadio(msg);
    }
    else if (msgName == "MosaicCellularConfigurationCmd")
    {
      auto *cmd = check_and_cast<MosaicCellularConfigurationCmd *>(msg);
      EV_DEBUG << "MosaicScenarioManager CELLULAR_CONF_RADIO" << std::endl;
      configureRadio(msg);
    }
    else if (msgName == "MosaicFinishCmd")
    {
      EV_DEBUG << "MosaicScenarioManager FINISH" << std::endl;
      delete msg; // Delete msg because finish() terminates the program.
      finish();
    }
    else
    {
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
  void MosaicScenarioManager::finish()
  {
    static bool once = false;
    if (!once)
    {
      // toggle the once flag
      once = true;

      // Tidy up and finish simulation
      while (nodes.begin() != nodes.end())
      {
        removeNode(nodes.begin()->first);
      }
      sched->endRun();
      EV << "MosaicScenarioManager simulation ended" << std::endl;
      endSimulation();
    }
  }

  cModule *MosaicScenarioManager::getManagedModule(int nodeId)
  {
    // Search for the node with the given nodeId in the nodes map
    auto it = nodes.find(nodeId);
    // Return the module pointer if found, nullptr otherwise
    return (it != nodes.end()) ? it->second : nullptr;
  }

  void MosaicScenarioManager::addModule(int nodeId, const std::string &type,
                                        const std::string &name,
                                        const std::string &displayString)
  {
    // Check if the nodeId already exists in the nodes map
    // If it does, throw an error to prevent duplicate module creation
    const auto [it, inserted] = nodes.try_emplace(nodeId, nullptr);
    if (!inserted)
    {
      error("Tried adding duplicate module");
    }

    auto *parentmod = getParentModule();
    if (!parentmod)
    {
      error("Parent module not found");
    }

    auto *nodeType = cModuleType::get(type.c_str());
    if (!nodeType)
    {
      error("Module type \"%s\" not found", type.c_str());
    }

    unsigned int nodeVectorIndex;

    // OMNeT++ 6.x: Ensure submodule vector exists before creating modules
    if (!parentmod->hasSubmoduleVector(name.c_str()))
    {
      // Add submodule vector with initial size to accommodate this node
      parentmod->addSubmoduleVector(name.c_str(), 1);
      EV_DEBUG << "Added submodule vector \"" << name << "\" with size " << (1)
         << std::endl;
      nodeVectorIndex = 0; // Reset index to 0 for new vector
    }
    else
    {
      nodeVectorIndex = parentmod->getSubmoduleVectorSize(name.c_str());
      // Expand existing vector if needed
      parentmod->setSubmoduleVectorSize(name.c_str(), nodeVectorIndex + 1);
      EV_DEBUG << "Expanded submodule vector \"" << name << "\" to size "
         << (nodeVectorIndex + 1) << std::endl;
    }
    EV_DEBUG << "Creating module of type \"" << type << "\" with name \"" << name
       << "\" at index " << nodeVectorIndex << " in parent module \""
       << parentmod->getFullName() << "\"" << std::endl;
    auto *mod = nodeType->create(name.c_str(), parentmod, nodeVectorIndex);

    // For gNodeB modules, we need to create a custom instance that uses
    // MosaicMobility Since the gNodeB hardcodes StationaryMobility, we'll handle
    // this in addNodeB method

    mod->getDisplayString().parse(displayString.c_str());

    // For gNodeB, don't schedule start yet - we need to replace mobility first
    if (type != nodeBModuleType)
    {

      mod->finalizeParameters();
      mod->buildInside();
      mod->scheduleStart(simTime());
    }
    else
    {
      EV_DEBUG << mod->par("nodeType") << " is a gNodeB, not scheduling start yet"
         << std::endl;
    }

    it->second = mod;
  }

  void MosaicScenarioManager::addNode(int nodeId, inet::Coord &position)
  {
    cModule *newNode = getManagedModule(nodeId);
    if (newNode)
    {
      error("Tried adding duplicate node (vehicle) %d", nodeId);
    }

    addModule(nodeId, vehModuleType, vehModuleName, moduleDisplayString);
    newNode = getManagedModule(nodeId);

    MosaicProxyApp *app;
    MosaicMobility *mobility;

    auto *submod = newNode->getSubmodule("proxyApp");
    if ((app = dynamic_cast<MosaicProxyApp *>(submod)))
    {
      // Ensure gate vector is large enough for nodeId
      if (this->gateSize("mosaicProxyOut") <= nodeId)
      {
        this->setGateSize("mosaicProxyOut", nodeId + 1);
      }
      if (this->gateSize("mosaicProxyIn") <= nodeId)
      {
        this->setGateSize("mosaicProxyIn", nodeId + 1);
      }
      this->gate("mosaicProxyOut", nodeId)->connectTo(app->gate("fedIn"));
      app->gate("fedOut")->connectTo(this->gate("mosaicProxyIn", nodeId));
      app->setExternalId(nodeId);
    }

    submod = newNode->getSubmodule("mobility");
    if ((mobility = dynamic_cast<MosaicMobility *>(submod)))
    {
      // Initialize mosaicmobility module (external id and initial position)
      mobility->setExternalId(nodeId);
      mobility->setNextPosition(position);
    }



    newNode->callInitialize();
    EV_DEBUG << "MosaicScenarioManager added vehicle " << nodeId << " at position "
       << position.x << "," << position.y << " at time " << simTime()
       << std::endl;
  }

  void MosaicScenarioManager::addRsuNode(int nodeId, inet::Coord &position)
  {
    cModule *newNode = getManagedModule(nodeId);
    if (newNode)
    {
      error("Tried adding duplicate node (rsu) %d", nodeId);
    }

    addModule(nodeId, rsuModuleType, rsuModuleName, moduleDisplayString);
    newNode = getManagedModule(nodeId);

    MosaicProxyApp *app;
    MosaicMobility *mobility;

    auto *submod = newNode->getSubmodule("proxyApp");

    if ((app = dynamic_cast<MosaicProxyApp *>(submod)))
    {
      // Initialize mosaicproxyapp module (external id and connection to fed)
      EV << "MosaicScenarioManager adding rsu node " << nodeId << " with proxyApp"
         << std::endl;
      // Ensure gate vector is large enough for nodeId
      if (this->gateSize("mosaicProxyOut") <= nodeId)
      {
        this->setGateSize("mosaicProxyOut", nodeId + 1);
      }
      if (this->gateSize("mosaicProxyIn") <= nodeId)
      {
        this->setGateSize("mosaicProxyIn", nodeId + 1);
      }
      this->gate("mosaicProxyOut", nodeId)->connectTo(app->gate("fedIn"));
      app->gate("fedOut")->connectTo(this->gate("mosaicProxyIn", nodeId));
      app->setExternalId(nodeId);
    }

    submod = newNode->getSubmodule("mobility");
    if ((mobility = dynamic_cast<MosaicMobility *>(submod)))
    {
      mobility->setExternalId(nodeId);
      mobility->setNextPosition(position);
    }

    newNode->callInitialize();
    EV_DEBUG << "MosaicScenarioManager added rsu " << nodeId << " at position "
             << position.x << "," << position.y << " at time " << simTime()
             << std::endl;
  }

  void MosaicScenarioManager::addNodeB(int nodeId, inet::Coord &position)
  {
    cModule *newNode = getManagedModule(nodeId);
    if (newNode)
    {
      error("Tried adding duplicate node (nodeB) %d", nodeId);
    }

    addModule(nodeId, nodeBModuleType, nodeBModuleName, moduleDisplayString);
    newNode = getManagedModule(nodeId);

    // Get the submodule vector for nodeBs
    auto *parentmod = getParentModule();
    if (!parentmod)
    {
      error("Parent module not found");
    }
    // auto newNode = parentmod->getSubmodule("nodeB", nodeId);
    if (newNode)
    {
      // Set display string with coordinates so ChannelAccess can find them as
      // fallback
      std::string displayStr = moduleDisplayString +
                               ";p=" + std::to_string((int)position.x) + "," +
                               std::to_string((int)position.y);
      newNode->getDisplayString().parse(displayStr.c_str());
    }

    // Replace the hardcoded StationaryMobility with MosaicMobility
    auto *oldMobility = newNode->getSubmodule("mobility");
    if (oldMobility)
    {
      // Delete the existing StationaryMobility module
      oldMobility->deleteModule();

      // Create a new MosaicMobility module
      auto *mobilityType =
          cModuleType::get("omnetpp_federate.node.MosaicMobility");
      if (mobilityType)
      {
        auto *newMobility = mobilityType->create("mobility", newNode);
        newMobility->finalizeParameters();
        newMobility->buildInside();

        // Set position and ID after building (parameters are now accessible)
        auto *mobility = dynamic_cast<MosaicMobility *>(newMobility);
        if (mobility)
        {
          mobility->setExternalId(nodeId);
          mobility->setNextPosition(position); // This sets the position properly

          EV_DEBUG << "MosaicMobility created for nodeB " << nodeId << " at position "
             << position.x << "," << position.y << std::endl;
        }
      }
    }
    // Now establish PPP connection to UPF for cellular traffic routing
    cModule *upf = getParentModule()->getSubmodule("upf");
    if (upf && newNode)
    {
      // Get the next available index for UPF pppg gates
      int upfPppIndex = upf->gateSize("pppg$o");
      EV << "Connecting gNodeB " << nodeId << " to UPF at pppg index "
         << upfPppIndex << std::endl;

      // Expand UPF gate vector to accommodate new connection
      upf->setGateSize("pppg", upfPppIndex + 1);

      cGate *gnbPppOut = newNode->gate("ppp$o");
      cGate *gnbPppIn = newNode->gate("ppp$i");
      cGate *upfPppOut = upf->gate("pppg$o", upfPppIndex);
      cGate *upfPppIn = upf->gate("pppg$i", upfPppIndex);

      // Create Ethernet channel for PPP connection (similar to Eth10G in
      // examples)
      cChannelType *channelType = cChannelType::get("inet.node.ethernet.Eth10G");
      if (channelType)
      {
        cChannel *channel1 = channelType->create("gNodeB-to-UPF");
        cChannel *channel2 = channelType->create("UPF-to-gNodeB");

        // Connect with proper channels: gNodeB.ppp <--> Eth10G <--> UPF.pppg[i]
        gnbPppOut->connectTo(upfPppIn, channel1);
        upfPppOut->connectTo(gnbPppIn, channel2);

        EV_DEBUG << "Connected gNodeB " << nodeId << " to UPF via pppg[" << upfPppIndex
           << "] with Eth10G channels for 5G core network routing" << std::endl;

        // Don't use rebuildInside() - create the PPP interface manually
        cModuleType *pppType = cModuleType::get("inet.linklayer.ppp.PppInterface");
        int pppIfaceIndex = upf->getSubmoduleVectorSize("ppp");
        EV << "Creating PPP interface ppp[" << pppIfaceIndex << "] in UPF"
           << std::endl;
        // Set the vector size for the ppp[] submodule vector
        upf->setSubmoduleVectorSize("ppp", pppIfaceIndex + 1);
        std::string pppName = "ppp" + std::to_string(pppIfaceIndex);
        cModule *newPppIf = pppType->create("ppp", upf, pppIfaceIndex);

        newPppIf->finalizeParameters();
        newPppIf->buildInside();

        // Connect to UPF's internal link interface dispatcher (li)
        cModule *li = upf->getSubmodule("li");
        int currentInSize = li->gateSize("in");
        int currentOutSize = li->gateSize("out");
        li->setGateSize("in", currentInSize + 1);
        li->setGateSize("out", currentOutSize + 1);
        cGate *pppUpperOut = newPppIf->gate("upperLayerOut");
        cGate *pppUpperIn = newPppIf->gate("upperLayerIn");
        cGate *liIn = li->gate("in", currentInSize);
        cGate *liOut = li->gate("out", currentOutSize);

        pppUpperOut->connectTo(liIn);
        liOut->connectTo(pppUpperIn);

        // Connect physical layer to external gate
        cGate *pppPhysOut = newPppIf->gate("phys$o");
        cGate *pppPhysIn = newPppIf->gate("phys$i");

        pppPhysOut->connectTo(upfPppOut);
        upfPppIn->connectTo(pppPhysIn);

        // Initialize PPP interface but don't start the NodeB yet
        newPppIf->scheduleStart(simTime());
        newPppIf->callInitialize();

        // DO NOT call newNode->scheduleStart() or newNode->callInitialize() yet
        // These will be called later in addAllNodeBsToSimulation()
      }
      else
      {
        EV_ERROR << "Failed to find Eth10G channel type for gNodeB-UPF connection"
                 << std::endl;
      }
    }

    // Store NodeB configuration for later batch initialization
    NodeBConfig config;
    config.nodeId = nodeId;
    config.position = position;
    pendingNodeBs.push_back(config);

    EV_DEBUG << "NodeB " << nodeId << " configured (but not started) at position " << position.x << "," << position.y
       << ". Total pending: " << pendingNodeBs.size() << std::endl;
  }

  void MosaicScenarioManager::moveNode(int nodeId, inet::Coord &position)
  {
    cModule *mod = getManagedModule(nodeId);
    if (mod == nullptr)
    {
      EV_WARN << "WARNING: Node " << nodeId << " not mapped" << std::endl;
    }
    else
    {
      for (cModule::SubmoduleIterator iter(mod); !iter.end(); iter++)
      {
        auto *submod = *iter;
        auto *mm = dynamic_cast<MosaicMobility *>(submod);
        if (mm == nullptr)
        {
          continue;
        }
        mm->setNextPosition(position);
      }
      EV_DEBUG << "MosaicScenarioManager moved vehicle " << nodeId
         << " to position " << position.x << "," << position.y
         << " at time " << simTime() << std::endl;
    }
  }

  void MosaicScenarioManager::removeNode(int nodeId)
  {
    cModule *mod = getManagedModule(nodeId);
    if (mod == nullptr)
    {
      error("No node with id %d found", nodeId);
    }
    else
    {
      EV << "MosaicScenarioManager removing node " << nodeId
         << " (OMNeT++ ID: " << mod->getId() << ") at time " << simTime()
         << std::endl;

      nodes.erase(nodeId);
      // we have to delete the two radios before we delete the node, since the
      // MediumLimitCache tries to determine the minimal constrained area from the
      // nonexistent mobility while deleting the radios.
      auto *wlan0Module = mod->getSubmodule("wlan0");
      if (wlan0Module != nullptr)
      {
        wlan0Module->deleteModule();
      }
      auto *wlan1Module = mod->getSubmodule("wlan1");
      if (wlan1Module != nullptr)
      {
        wlan1Module->deleteModule();
      }

      mod->callFinish();
      mod->deleteModule();

      EV_DEBUG << "MosaicScenarioManager removed node " << nodeId << " at time "
         << simTime() << std::endl;
    }
  }

  void MosaicScenarioManager::sendV2xMessage(cMessage *msg)
  {
    auto *cmd = check_and_cast<MosaicCommunicationCmd *>(msg);
    int nodeId = cmd->getNodeId();
    cModule *mod = getManagedModule(nodeId);
    if (mod == nullptr)
    {
      EV << "WARNING: Node " << nodeId << " not mapped" << std::endl;
    }
    else
    {
      // Check if WiFi or cellular based on command type
      bool isCellular =
          (cmd->getCmdType() == COMMUNICATION_CMD_SEND_CELL_MESSAGE);

      if (!isCellular && cmd->getTtl() != 1)
      {
        EV << "WARNING: up to now only singlehop-broadcast supported for WiFi"
           << std::endl;
      }
      else
      {
        const auto msgId = cmd->getMsgId();
        const auto packetName = "V2xPacket" + std::to_string(msgId);
        auto *packet = new MosaicAppPacket(packetName.c_str());
        packet->setNodeId(nodeId);
        packet->setMsgId(msgId);
        packet->setDestAddr(cmd->getDestAddr());

        // Set channel: -1 for cellular, actual channel for WiFi
        if (isCellular)
        {
          packet->setChannelId(-1); // Cellular indicator
        }
        else
        {
          packet->setChannelId(cmd->getChannelId()); // WiFi channel
        }

        const auto chunk =
            inet::makeShared<inet::ByteCountChunk>(cmd->getLength());
        packet->insertAtFront(chunk);

        send(packet, "mosaicProxyOut", nodeId);
        EV_DEBUG << "MosaicScenarioManager send " << (isCellular ? "cellular" : "wifi")
           << " message " << msgId << " from node " << nodeId << " at time "
           << simTime() << std::endl;
      }
    }
  }

  void MosaicScenarioManager::receiveV2xMessage(cMessage *msg)
  {
    auto *packet = check_and_cast<MosaicAppPacket *>(msg);
    EV_DEBUG << "MosaicScenarioManager receive v2x message " << packet->getMsgId()
             << " on node " << packet->getNodeId() << " at time " << simTime()
             << " on channel " << packet->getChannelId() << std::endl;
    sched->reportReceivedV2xMessage(msg);
  }

  void MosaicScenarioManager::configureRadio(cMessage *msg)
  {
    auto *cmd = check_and_cast<MosaicConfigurationCmd *>(msg);
    if (cmd == nullptr)
    {
      return;
    }
    int nodeId = cmd->getNodeId();
    EV_DEBUG << "MosaicScenarioManager configure radio on node " << nodeId
       << std::endl;

    cModule *mod = getManagedModule(nodeId);
    if (mod == nullptr)
    {
      EV_WARN << "WARNING: Node " << nodeId << " not mapped" << std::endl;
    }
    else
    {
      send(msg->dup(), "mosaicProxyOut", nodeId);
    }
  }

  void MosaicScenarioManager::addWiredNode(int nodeId, inet::Coord &position)
  {
    cModule *newNode = getManagedModule(nodeId);
    if (newNode)
    {
      error("Tried adding duplicate node (wired) %d", nodeId);
    }

    addModule(nodeId, wiredModuleType, wiredModuleName, moduleDisplayString);
    newNode = getManagedModule(nodeId);

    MosaicProxyApp *app;
    MosaicMobility *mobility;

    auto *submod = newNode->getSubmodule("proxyApp");
    if ((app = dynamic_cast<MosaicProxyApp *>(submod)))
    {
      EV << "MosaicScenarioManager adding wired node " << nodeId
         << " with proxyApp" << std::endl;
      EV << "Current gate size: mosaicProxyOut="
         << this->gateSize("mosaicProxyOut") << ", mosaicProxyIn="
         << this->gateSize("mosaicProxyIn") << std::endl;
      // Ensure gate vector is large enough for nodeId
      if (this->gateSize("mosaicProxyOut") <= nodeId)
      {
        this->setGateSize("mosaicProxyOut", nodeId + 1);
      }
      if (this->gateSize("mosaicProxyIn") <= nodeId)
      {
        this->setGateSize("mosaicProxyIn", nodeId + 1);
      }
      this->gate("mosaicProxyOut", nodeId)->connectTo(app->gate("fedIn"));
      app->gate("fedOut")->connectTo(this->gate("mosaicProxyIn", nodeId));
      app->setExternalId(nodeId);
    }

    submod = newNode->getSubmodule("mobility");
    if ((mobility = dynamic_cast<MosaicMobility *>(submod)))
    {
      mobility->setExternalId(nodeId);
      mobility->setNextPosition(position);
    }

    // Connect wired node to Ethernet switch for network connectivity
    auto *parentmod = getParentModule();
    auto *ethernetSwitch = parentmod->getSubmodule("serverGateway");
    if (ethernetSwitch && newNode)
    {
      // Get the next available index for switch gates
      int gateSize = ethernetSwitch->gateSize("ethg");
      EV_DEBUG << "Ethernet switch current ethg gate size: " << gateSize << std::endl;
      ethernetSwitch->setGateSize("ethg", gateSize); // Ensure gate size is set
      int switchEthIndex = gateSize;                 // Next available index
      EV_DEBUG << "Connecting wired node " << nodeId
         << " to Ethernet switch at ethg index " << switchEthIndex << std::endl;

      // Expand switch gate vector to accommodate new connection
      ethernetSwitch->setGateSize("ethg", gateSize + 1);

      cGate *nodeEthOut = newNode->gate("ethg$o");
      cGate *nodeEthIn = newNode->gate("ethg$i");
      cGate *switchEthOut = ethernetSwitch->gate("ethg$o", switchEthIndex);
      cGate *switchEthIn = ethernetSwitch->gate("ethg$i", switchEthIndex);

      // Create Ethernet channel for connection
      cChannelType *channelType = cChannelType::get("inet.node.ethernet.Eth10G");
      if (channelType)
      {
        cChannel *channel1 = channelType->create("wired-to-switch");
        cChannel *channel2 = channelType->create("switch-to-wired");

        // Connect with proper channels: WiredNode.ethg <--> Eth10G <--> EthernetSwitch.ethg[i]
        nodeEthOut->connectTo(switchEthIn, channel1);
        switchEthOut->connectTo(nodeEthIn, channel2);

        EV_DEBUG << "Connected wired node " << nodeId << " to Ethernet switch via ethg[" << switchEthIndex
           << "] with Eth10G channels for network connectivity" << std::endl;

        // Create Ethernet interface on the switch for this connection
        cModuleType *ethType = cModuleType::get("inet.linklayer.ethernet.EthernetInterface");
        if (ethType)
        {
          int ethIfaceIndex = ethernetSwitch->getSubmoduleVectorSize("eth");
          EV << "Creating Ethernet interface eth[" << ethIfaceIndex << "] on switch" << std::endl;

          // Set the vector size for the eth[] submodule vector
          ethernetSwitch->setSubmoduleVectorSize("eth", ethIfaceIndex + 1);
          cModule *newEthIf = ethType->create("eth", ethernetSwitch, ethIfaceIndex);

          newEthIf->finalizeParameters();
          newEthIf->buildInside();

          // Connect to switch's internal link interface dispatcher (li)
          cModule *li = ethernetSwitch->getSubmodule("li");
          if (li)
          {
            int currentInSize = li->gateSize("in");
            int currentOutSize = li->gateSize("out");
            li->setGateSize("in", currentInSize + 1);
            li->setGateSize("out", currentOutSize + 1);

            cGate *ethUpperOut = newEthIf->gate("upperLayerOut");
            cGate *ethUpperIn = newEthIf->gate("upperLayerIn");
            cGate *liIn = li->gate("in", currentInSize);
            cGate *liOut = li->gate("out", currentOutSize);

            ethUpperOut->connectTo(liIn);
            liOut->connectTo(ethUpperIn);
          }

          // Connect physical layer to external gate
          cGate *ethPhysOut = newEthIf->gate("phys$o");
          cGate *ethPhysIn = newEthIf->gate("phys$i");


          ethPhysOut->connectTo(switchEthOut);
          switchEthIn->connectTo(ethPhysIn);

          // Initialize the Ethernet interface
          newEthIf->scheduleStart(simTime());
          newEthIf->callInitialize();

          EV << "Ethernet interface created on switch for wired node " << nodeId << std::endl;
        }
        else
        {
          EV_ERROR << "Failed to find EthernetInterface type for switch connection" << std::endl;
        }
      }
      else
      {
        EV_ERROR << "Failed to find Eth10G channel type for wired node-switch connection" << std::endl;
      }
      newNode->scheduleStart(simTime());
      newNode->callInitialize();
    }

    EV_DEBUG << "MosaicScenarioManager added wired node " << nodeId << " at position "
       << position.x << "," << position.y << " at time " << simTime()
       << std::endl;
  }

  void MosaicScenarioManager::configureX2Connections()
  {
    EV << "Configuring X2 connections between " << pendingNodeBs.size() << " NodeBs" << std::endl;

    if (pendingNodeBs.size() < 2)
    {
      EV_DEBUG << "Less than 2 NodeBs, no X2 connections needed" << std::endl;
    }

    // Create physical X2 connections between NodeBs
    for (size_t i = 0; i < pendingNodeBs.size(); i++)
    {
      for (size_t j = i + 1; j < pendingNodeBs.size(); j++)
      {
        int nodeB1 = pendingNodeBs[i].nodeId;
        int nodeB2 = pendingNodeBs[j].nodeId;

        // Calculate distance between NodeBs
        double distance = sqrt(pow(pendingNodeBs[i].position.x - pendingNodeBs[j].position.x, 2) +
                               pow(pendingNodeBs[i].position.y - pendingNodeBs[j].position.y, 2));

        EV_DEBUG << "Creating X2 connection between NodeB " << nodeB1
                 << " and NodeB " << nodeB2 << " (distance: " << distance << "m)" << std::endl;

        createX2Connection(nodeB1, nodeB2);
      }
    }

    // Now build all NodeBs so we can access their submodules
    for (const auto &config : pendingNodeBs)
    {
      cModule *nodeB = getManagedModule(config.nodeId);
      if (nodeB)
      {
        nodeB->buildInside();
      }
    }

    // Create mapping of nodeB IDs to their index for X2App configuration
    std::map<int, int> nodeBIdToIndex;
    for (size_t i = 0; i < pendingNodeBs.size(); i++)
    {
      nodeBIdToIndex[pendingNodeBs[i].nodeId] = i;
    }

    // Configure X2Apps for each nodeB
    for (size_t i = 0; i < pendingNodeBs.size(); i++)
    {
      cModule *nodeB = getManagedModule(pendingNodeBs[i].nodeId);
      if (!nodeB)
        continue;

      int x2AppIndex = 0;

      // Configure X2App for each peer nodeB
      for (size_t j = 0; j < pendingNodeBs.size(); j++)
      {
        if (i == j)
          continue; // Skip self

        int peerNodeId = pendingNodeBs[j].nodeId;
        EV_DEBUG << "Configuring X2App[" << x2AppIndex << "] on NodeB " << pendingNodeBs[i].nodeId
                 << " to connect to NodeB " << peerNodeId << std::endl;

        // Get the X2App instance
        auto *x2App = nodeB->getSubmodule("x2App", x2AppIndex);
        if (x2App)
        {
          auto *client = x2App->getSubmodule("client");
          auto *server = x2App->getSubmodule("server");

          if (server)
          {
            // Set unique port for this X2App server
            server->par("localPort") = 5000 + x2AppIndex;
            EV_DEBUG << "  Server port set to: " << (5000 + x2AppIndex) << std::endl;
          }

          if (client)
          {
            // Set connectAddress to peer nodeB's x2ppp interface
            // Format: "gnb[peerid]$x2ppp$index"
            int peerIndex = nodeBIdToIndex[peerNodeId];
            int peerX2AppIndex = getX2AppIndexForPeer(peerNodeId, pendingNodeBs[i].nodeId);
            std::string connectAddr = "gnb[" + std::to_string(peerIndex) + "]%x2ppp" + std::to_string(peerX2AppIndex);
            client->par("connectAddress") = connectAddr.c_str();
            EV_DEBUG << "  Client connectAddress set to: " << connectAddr << std::endl;
          }
        }

        x2AppIndex++;
      }
    }
  }

  void MosaicScenarioManager::createX2Connection(int nodeB1Id, int nodeB2Id)
  {
    cModule *nodeB1 = getManagedModule(nodeB1Id);
    cModule *nodeB2 = getManagedModule(nodeB2Id);

    if (!nodeB1 || !nodeB2)
    {
      EV_ERROR << "Cannot create X2 connection: NodeB " << nodeB1Id
               << " or NodeB " << nodeB2Id << " not found" << std::endl;
      return;
    }

    try
    {
      // Use dedicated X2 gates for NodeB-to-NodeB connections
      // Get current number of X2 connections to add new ones
      int nodeB1X2Count = nodeB1->gateSize("x2$o");
      int nodeB2X2Count = nodeB2->gateSize("x2$o");

      // Expand X2 gate vectors for new connections
      nodeB1->setGateSize("x2", nodeB1X2Count + 1);
      nodeB2->setGateSize("x2", nodeB2X2Count + 1);

      // Get the new X2 gates for connection
      cGate *nodeB1X2Out = nodeB1->gate("x2$o", nodeB1X2Count);
      cGate *nodeB1X2In = nodeB1->gate("x2$i", nodeB1X2Count);
      cGate *nodeB2X2Out = nodeB2->gate("x2$o", nodeB2X2Count);
      cGate *nodeB2X2In = nodeB2->gate("x2$i", nodeB2X2Count);

      // Create Ethernet channels for X2 connection
      cChannelType *channelType = cChannelType::get("inet.node.ethernet.Eth10G");
      if (channelType)
      {
        cChannel *channel1 = channelType->create("X2-nodeB1-to-nodeB2");
        cChannel *channel2 = channelType->create("X2-nodeB2-to-nodeB1");

        // Connect the NodeBs directly via X2: nodeB1.x2[i] <--> nodeB2.x2[j]
        nodeB1X2Out->connectTo(nodeB2X2In, channel1);
        nodeB2X2Out->connectTo(nodeB1X2In, channel2);

        EV << "X2 connection established: NodeB " << nodeB1Id << ".x2[" << nodeB1X2Count
           << "] <--> NodeB " << nodeB2Id << ".x2[" << nodeB2X2Count << "]" << std::endl;

        // Check the x2ppp submodules vector to check the size
        auto *x2ppp1 = nodeB1->getSubmodule("x2ppp");
        auto *x2ppp2 = nodeB2->getSubmodule("x2ppp");
        if (!x2ppp1)
        {
          EV_ERROR << "Failed to find x2ppp submodule for NodeB " << nodeB1Id << std::endl;
        }
        if (!x2ppp2)
        {
          EV_ERROR << "Failed to find x2ppp submodule for NodeB " << nodeB2Id << std::endl;
        }
        if (x2ppp1 && x2ppp2)
        {
          int x2ppp1Size = nodeB1->getSubmoduleVectorSize("x2ppp");
          int x2ppp2Size = nodeB2->getSubmoduleVectorSize("x2ppp");
          EV << "NodeB " << nodeB1Id << " x2ppp size: " << x2ppp1Size << std::endl;
          EV << "NodeB " << nodeB2Id << " x2ppp size: " << x2ppp2Size << std::endl;
        }
      }
      else
      {
        EV_ERROR << "Failed to find Eth10G channel type for X2 connection" << std::endl;
      }
    }
    catch (const std::exception &e)
    {
      EV_ERROR << "Error creating X2 connection between NodeB " << nodeB1Id
               << " and NodeB " << nodeB2Id << ": " << e.what() << std::endl;
    }
  }

  void MosaicScenarioManager::addAllNodeBsToSimulation()
  {
    EV << "Adding all " << pendingNodeBs.size() << " configured NodeBs to simulation simultaneously" << std::endl;

    // Add all configured NodeBs to the simulation
    for (const auto &config : pendingNodeBs)
    {
      EV_DEBUG << "Adding NodeB " << config.nodeId << " at position "
         << config.position.x << "," << config.position.y << " to simulation" << std::endl;
      // addNodeBToSimulation(config.nodeId, const_cast<inet::Coord&>(config.position));
      cModule *newNode = getManagedModule(config.nodeId);
      if (!newNode)
      {
        error("NodeB %d not found for simulation initialization", config.nodeId);
      }

    }

    int numInitStages = inet::NUM_INIT_STAGES;
    // Initialize all NodeBs after they are added
    bool reconfigured = false;
    for (int stage = 0; stage < inet::INITSTAGE_NETWORK_LAYER; ++stage)
    {
      for (const auto &config : pendingNodeBs)
      {
        cModule *newNode = getManagedModule(config.nodeId);
        if (newNode)
        {
          newNode->callInitialize(stage);
        }
      }
    }
    auto *parentmod = getParentModule();
    auto *ipconfig = parentmod->getSubmodule("configurator");
    if (ipconfig)
    {
      ipconfig->deleteModule();
    }
    cModuleType *configuratorType = cModuleType::get("inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator");
    cModule *newConfigurator = configuratorType->create("configurator", parentmod);

    // Set parameters (copy from original configuration)
    // newConfigurator->par("config") = "xmldoc(\"network_config.xml\")";
    newConfigurator->finalizeParameters();
    newConfigurator->buildInside();
    newConfigurator->callInitialize();

    cModule *configurator = getParentModule()->getSubmodule("configurator");
    if (configurator)
    {
      auto *configNode = dynamic_cast<inet::Ipv4NetworkConfigurator *>(configurator);
      configNode->configureAllInterfaces();
      configNode->configureAllRoutingTables();
    }

    for (int stage = inet::INITSTAGE_NETWORK_LAYER; stage < numInitStages; ++stage)
    {
      for (const auto &config : pendingNodeBs)
      {
        cModule *newNode = getManagedModule(config.nodeId);
        if (newNode)
        {
          newNode->callInitialize(stage);
        }
      }
    }

    for (const auto &config : pendingNodeBs)
    {
      cModule *newNode = getManagedModule(config.nodeId);
      if (newNode)
      {
        newNode->scheduleStart(simTime());
      }
    }

    // Clear the pending list as all NodeBs have been added
    pendingNodeBs.clear();

    EV << "All NodeBs successfully added to simulation with X2Apps configured" << std::endl;
  }

  void MosaicScenarioManager::addNodeBToSimulation(int nodeId, inet::Coord &position)
  {
    cModule *newNode = getManagedModule(nodeId);
    if (!newNode)
    {
      error("NodeB %d not found for simulation initialization", nodeId);
    }
    // newNode->par("numX2Apps") = 1;
    newNode->buildInside();

    // Get the interface and routing table
    auto *ift = check_and_cast<inet::IInterfaceTable *>(
        newNode->getSubmodule("interfaceTable"));
    auto *rt = check_and_cast<inet::Ipv4RoutingTable *>(
        newNode->getSubmodule("ipv4")->getSubmodule("routingTable"));

    // Get the global IP configurator
    // auto *ipconfig = check_and_cast<inet::Ipv4NetworkConfigurator *>(
    //     getParentModule()->getSubmodule("configurator"));
    // if (ipconfig)
    // {
    //   EV << "Starting IP configuration for gNodeB " << nodeId << std::endl;
    //   if (ift && rt)
    //   {
    //     EV << "Found interface table with " << ift->getNumInterfaces() << " interfaces and routing table" << std::endl;
    //     for (int i = 0; i < ift->getNumInterfaces(); i++)
    //     {
    //       auto *iface = ift->getInterface(i);
    //       EV << "Configuring interface " << i << ": " << iface->getInterfaceName() << std::endl;
    //       ipconfig->configureInterface(iface);
    //       EV << "Interface " << i << " configured successfully" << std::endl;
    //       // Print assigned IP address
    //       EV << "Interface details: " << iface << std::endl;
    //     }
    //     EV << "Configuring routing table for gNodeB " << nodeId << std::endl;
    //     ipconfig->configureRoutingTable(rt);
    //     EV << "Routing table configured successfully for gNodeB " << nodeId << std::endl;
    //     EV << "Routing table : " << rt << std::endl;
    //   }
    //   else
    //   {
    //     EV_ERROR << "Failed to get interface table or routing table for gNodeB " << nodeId << std::endl;
    //   }
    // }
    // else
    // {
    //   EV_ERROR << "IP configurator not found for gNodeB " << nodeId << std::endl;
    // }

    // EV << "NodeB " << nodeId << " successfully added to simulation" << std::endl;
  }

  int MosaicScenarioManager::getX2AppIndexForPeer(int peerNodeId, int sourceNodeId)
  {
    // Find the peer node configuration
    auto peerIt = std::find_if(pendingNodeBs.begin(), pendingNodeBs.end(),
                               [peerNodeId](const NodeBConfig &config)
                               {
                                 return config.nodeId == peerNodeId;
                               });

    if (peerIt == pendingNodeBs.end())
    {
      EV_ERROR << "getX2AppIndexForPeer: Peer nodeId " << peerNodeId << " not found" << std::endl;
      return -1;
    }

    // Calculate X2App index for the peer node based on the order of connections
    // The peer's X2App index is determined by counting how many NodeBs (within range)
    // come before the source node in the peer's connection list
    int x2AppIndex = 0;
    const double maxX2Distance = 2000.0;

    for (const auto &config : pendingNodeBs)
    {
      if (config.nodeId == peerNodeId)
        continue; // Skip the peer itself

      // Calculate distance between peer and this potential connection
      double distance = sqrt(pow(peerIt->position.x - config.position.x, 2) +
                             pow(peerIt->position.y - config.position.y, 2));

      if (distance <= maxX2Distance)
      {
        // If this is the source node, return the current index
        if (config.nodeId == sourceNodeId)
        {
          return x2AppIndex;
        }
        x2AppIndex++;
      }
    }

    EV_ERROR << "getX2AppIndexForPeer: Could not find X2App index for connection from "
             << sourceNodeId << " to " << peerNodeId << std::endl;
    return -1;
  }

} // namespace omnetpp_federate
