require("premake", ">=5.0.0-beta4")

local buildpath = require "common/buildpath"
local cflags    = require "common/cflags"

include "pulsar"
include "pulsar-bindings"
include "../libs/cppdap/cppdap"

project "pulsar-debugger"
  kind "StaticLib"
  language "C++"
  cppdialect "C++20"

  buildpath.setup("pulsar-debugger")

  includedirs "../include"
  files { "../src/pulsar-debugger/**.cpp", "../include/pulsar-debugger/**.h" }
  links { "pulsar-bindings", "pulsar", "cppdap" }

  cflags()
