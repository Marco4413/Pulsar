require("premake", ">=5.0.0-beta4")

local buildpath = require "common/buildpath"
local cflags    = require "common/cflags"

include "cpulsar"
include "pulsar-core"

project "pulsar-tools-ext"
  kind "SharedLib"
  language "C"
  cdialect "C99"

  buildpath.setup("pulsar-tools-ext")

  includedirs { "../include" }
  files { "../src/pulsar-tools-ext/**.c", "../include/pulsar-tools-ext/**.h" }
  links { "cpulsar", "pulsar-core" }

  cflags()

  pic "On"

  filter "system:windows"
    defines "CPULSAR_WINDOWS"

  filter "system:linux"
    defines "CPULSAR_UNIX"
