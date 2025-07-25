require("premake", ">=5.0.0-beta4")

local buildpath = require "common/buildpath"
local cflags    = require "common/cflags"

include "pulsar"
include "../libs/cppdap/cppdap"

project "pulsar-debugger"
  kind "ConsoleApp"
  language "C++"
  cppdialect "C++20"

  buildpath.setup("pulsar-debugger")

  includedirs {
    "../include",
    "../libs/cppdap/include",
  }
  files { "../src/pulsar-debugger/**.cpp", "../include/pulsar-debugger/**.h" }
  links { "pulsar", "cppdap" }

  cflags()
