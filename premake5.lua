-- premake5.lua

local install_prefix = "/usr/local"

--local pkgconfig = require 'pkgconfig'
--print(pkgconfig.load('zlib'))
--print(pkgconfig.load('glib-2.0'))

local autoconf = require 'autoconf'
local tools = require 'tools'
local PROTO_PATH    = "."
local PROTO_CC_PATH = "."

autoconfigure {
    ['config.h'] = function (cfg)
        check_include(cfg, 'HAVE_PTHREAD_H', 'pthread.h')
    end
}

local PROTOC_BIN = tools.check_bin ( 'protoc' )
local OPP_MSGC_BIN = tools.check_bin ( 'opp_msgc' )
local INSTALL_BIN = tools.check_bin ( 'install' )
local MKDIR_BIN = tools.check_bin ( 'mkdir' )


workspace "omnetpp-federate"
   configurations { "Debug", "Release" }
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"

newoption {
   trigger     = "generate-opp-messages",
   description = "Generate/Regenerate opp messages with opp message compiler"
}

newoption {
   trigger     = "generate-protobuf",
   description = "Generate/Regenerate protocol buffers with protobuf compiler"
}

newoption {
   trigger     = "install",
   description = "install target into '" .. install_prefix .. "'"
}

-- -------------------------------------
-- target: lib/libomnetpp-federate.so --
-- -------------------------------------

project "omnetpp-federate-LIBRARY"

  targetname "omnetpp-federate"
  kind "SharedLib"
  files { "src/node/*.h"
        , "src/node/*.cc"
        , "src/msg/MosaicAppPacket_m.h"
        , "src/msg/MosaicAppPacket_m.cc"
        , "src/msg/MosaicCommunicationCmd_m.h"
        , "src/msg/MosaicCommunicationCmd_m.cc"
        , "src/msg/MosaicConfigurationCmd_m.h"
        , "src/msg/MosaicConfigurationCmd_m.cc"
        , "src/msg/MosaicMobilityCmd_m.h"
        , "src/msg/MosaicMobilityCmd_m.cc"
        , "src/mgmt/*.h" 
        , "src/mgmt/*.cc" 
        , PROTO_CC_PATH .. "/ClientServerChannel.h"
        , PROTO_CC_PATH .. "/ClientServerChannel.cc"
        , PROTO_CC_PATH .. "/ClientServerChannelMessages.pb.h"
        , PROTO_CC_PATH .. "/ClientServerChannelMessages.pb.cc"
        }

  includedirs { "/usr/include"
              , "src"
              , "src/mgmt"
              , "src/msg"
              , "src/node"
              , "src/util"
              , PROTO_CC_PATH
              }

  libdirs { "/usr/lib" }

  buildoptions { "-std=c++11" }
  links { "protobuf" }

  filter "configurations:Debug"
     defines { "DEBUG" }
     symbols "On"

  filter "configurations:Release"
     defines { "NDEBUG" }
     optimize "On"

  configuration "generate-opp-messages"
     prebuildcommands { OPP_MSGC_BIN .. " --msg6 -I /usr/lib" .. " src/msg/MosaicCommunicationCmd.msg"
                      , OPP_MSGC_BIN .. " --msg6 -I /usr/lib" .. " src/msg/MosaicConfigurationCmd.msg"
                      , OPP_MSGC_BIN .. " --msg6 -I /usr/lib" .. " src/msg/MosaicMobilityCmd.msg"
                      , OPP_MSGC_BIN .. " --msg6 -I /usr/lib" .. " src/msg/MosaicAppPacket.msg"
                      }

  configuration "generate-protobuf"
     prebuildcommands { PROTOC_BIN .. " --cpp_out=" .. PROTO_CC_PATH
                        .. " --proto_path=" .. PROTO_PATH
                        .." ClientServerChannelMessages.proto"
                      }

  configuration "install"
     postbuildcommands { MKDIR_BIN .. " -p " .. install_prefix .. "/lib"
                       , INSTALL_BIN .. " bin/%{cfg.buildcfg}/libomnetpp-federate.so " .. install_prefix .. "/lib" --strip
                       , MKDIR_BIN .. " -p " .. install_prefix .. "/share/ned/omnetpp_federate"
                       , "cp ./src/package.ned " .. install_prefix .. "/share/ned/omnetpp_federate"
                       , MKDIR_BIN .. " -p " .. install_prefix .. "/share/ned/omnetpp_federate/mgmt"
                       , "cp ./src/mgmt/MosaicScenarioManager.ned " .. install_prefix .. "/share/ned/omnetpp_federate/mgmt"
                       , "cp ./src/mgmt/Simulation.ned " .. install_prefix .. "/share/ned/omnetpp_federate/mgmt"
                       , MKDIR_BIN .. " -p " .. install_prefix .. "/share/ned/omnetpp_federate/node"
                       , "cp ./src/node/Rsu.ned " .. install_prefix .. "/share/ned/omnetpp_federate/node"
                       , "cp ./src/node/Vehicle.ned " .. install_prefix .. "/share/ned/omnetpp_federate/node"
                       , "cp ./src/node/MosaicProxyApp.ned " .. install_prefix .. "/share/ned/omnetpp_federate/node"
                       , "cp ./src/node/MosaicMobility.ned " .. install_prefix .. "/share/ned/omnetpp_federate/node"
                       }

  configuration "Debug"
     libdirs { "bin/Debug" }

  configuration "Release"
     libdirs { "bin/Release" }

-- -------------------------------
-- target: bin/omnetpp-federate --
-- -------------------------------

project "omnetpp-federate-BINARY"
   targetname "omnetpp-federate"
   kind "ConsoleApp"

   files {"src/omnetpp-federate/**.h"}
   buildoptions { "-std=c++17" }
   linkoptions { "-Wl,--no-as-needed,-rpath,'$$ORIGIN/lib',-rpath,'$$ORIGIN/../lib'" }

   includedirs { "/usr/include"
               , "src"
               , "src/mgmt"
               , "src/msg"
               , "src/node"
               , "src/util"
               }

   filter "configurations:Debug"
       defines { "DEBUG" }
       links { "INET_dbg"
             , "omnetpp-federate-LIBRARY"
    --       , "oppsim_dbg"
    --       , "oppscave_dbg"
    --       , "oppnedxml_dbg"
             , "oppmain_dbg"
    --       , "oppeventlog_dbg"
             , "oppenvir_dbg"
             , "dl"
    --       , "oppcommon_dbg"
             , "oppcmdenv_dbg"
             }
      symbols "On"

   filter "configurations:Release"
       defines { "NDEBUG" }
       links { "INET"
             , "omnetpp-federate-LIBRARY"
    --       , "oppsim"
    --       , "oppscave"
    --       , "oppnedxml"
             , "oppmain"
    --       , "oppeventlog"
             , "oppenvir"
             , "dl"
    --       , "oppcommon"
             , "oppcmdenv"
             }
      optimize "On"

  configuration "install"
     postbuildcommands { MKDIR_BIN .. " -p " .. install_prefix .. "/bin"
                       , INSTALL_BIN .. " bin/%{cfg.buildcfg}/omnetpp-federate " .. install_prefix .. "/bin" --strip
                       }

   configuration "Debug"
      libdirs { "bin/Debug", "/usr/lib" }

   configuration "Release"
      libdirs { "bin/Release", "/usr/lib" }

if _ACTION == "clean" then
    os.rmdir("bin")
    os.rmdir("obj")
    os.rmdir("omnetpp-federate-BINARY.make");
    os.rmdir("omnetpp-federate-LIBRARY.make");
end
