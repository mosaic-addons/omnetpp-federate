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

#ifndef MOSAICSCENARIOMANAGER_H_
#define MOSAICSCENARIOMANAGER_H_

#include <omnetpp.h>

#include "mgmt/MosaicEventScheduler.h"
#include "util/ClientServerChannel.h"

#include "inet/common/INETDefs.h"
#include "inet/common/geometry/common/Coord.h"
#include "inet/common/ModuleAccess.h"
#include "inet/networklayer/contract/ipv4/Ipv4Address.h"

namespace omnetpp_federate {

class MosaicScenarioManager : public cSimpleModule {

public:
    MosaicScenarioManager() = default;
    virtual ~MosaicScenarioManager() = default;

    virtual void initialize();
    virtual void finish();
    virtual void handleMessage(cMessage *msg);

private:
    std::string vehModuleType;
    std::string vehModuleName;
    std::string rsuModuleType;
    std::string rsuModuleName;
    std::string moduleDisplayString;
    inet::Ipv4Address addressBase;
    inet::Ipv4Address netmask;
    std::map<int, cModule*> nodes;
    MosaicEventScheduler* sched;

    virtual cModule* getManagedModule(int nodeId);
    virtual void addModule(int nodeId, std::string type, std::string name, std::string displayString);
    virtual void addNode(int nodeId, inet::Coord& position);
    virtual void addRsuNode(int nodeId, inet::Coord& position);
    virtual void moveNode(int nodeId, inet::Coord& position);
    virtual void removeNode(int nodeId);
    virtual void sendV2xMessage(cMessage *msg);
    virtual void receiveV2xMessage(cMessage *msg);
    virtual void configureRadio(cMessage *msg);
};

} //namespace omnetpp_federate

#endif
