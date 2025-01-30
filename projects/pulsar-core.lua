require("premake", ">=5.0.0-beta4")

local buildpath = require "common/buildpath"
local cflags    = require "common/cflags"

project "pulsar-core"
  kind "SharedLib"
  language "C++"
  cppdialect "C++20"

  buildpath.setup("pulsar-core")

  includedirs "../include"
  files { "../src/pulsar/core.cpp", "../include/pulsar/core.h" }

  cflags()

  pic "On"
