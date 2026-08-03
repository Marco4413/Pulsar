require("premake", ">=5.0.0-beta4")

local buildpath = require "common/buildpath"
local cflags    = require "common/cflags"

include "pulsar-debugger"
include "../libs/cppdap/cppdap"

project "pulsar-dap"
  kind "ConsoleApp"
  language "C++"
  cppdialect "C++20"

  buildpath.setup("pulsar-dap")

  includedirs { "../include", "../libs/cppdap/include" }
  files { "../src/pulsar-dap/**.cpp", "../include/pulsar-dap/**.h" }
  links { "pulsar-debugger", "pulsar-bindings", "pulsar", "cppdap" }

  cflags()
