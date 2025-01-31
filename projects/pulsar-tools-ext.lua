require("premake", ">=5.0.0-beta4")

local buildpath = require "common/buildpath"
local cflags    = require "common/cflags"

include "cpulsar"

project "pulsar-tools-ext"
  kind "SharedLib"
  language "C"
  cdialect "C99"

  defines "CPULSAR_SHAREDLIB"

  buildpath.setup("pulsar-tools-ext")

  includedirs { "../include" }
  files { "../src/pulsar-tools-ext/**.c", "../include/pulsar-tools-ext/**.h" }
  links { "cpulsar" }

  cflags()

  pic "On"

  filter "system:linux"
    defines "CPULSAR_UNIX"

  filter "system:windows"
    defines "CPULSAR_WINDOWS"
