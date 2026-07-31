require("premake", ">=5.0.0-beta4")

local buildpath = require "common/buildpath"
local cflags    = require "common/cflags"

include "pulsar"

project "pulsar-bindings"
  kind "StaticLib"
  language "C++"
  cppdialect "C++20"

  buildpath.setup("pulsar-bindings")

  includedirs "../include"
  files { "../src/pulsar-bindings/**.cpp", "../include/pulsar-bindings/**.h" }
  links "pulsar"

  cflags()
