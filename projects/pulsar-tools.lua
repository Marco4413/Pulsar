require("premake", ">=5.0.0-beta4")

local buildpath = require "common/buildpath"
local cflags    = require "common/cflags"

include "pulsar"

project "pulsar-tools"
  kind "ConsoleApp"
  language "C++"
  cppdialect "C++20"

  buildpath.setup("pulsar-tools")

  includedirs { "../include", "../libs/argue" }
  files {
    "../src/pulsar-tools/**.cpp", "../include/pulsar-tools/**.h",
    "../libs/argue/argue.hpp"
  }
  links "pulsar"

  cflags()
