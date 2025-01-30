require("premake", ">=5.0.0-beta4")

local buildpath = require "common/buildpath"
local cflags    = require "common/cflags"

include "pulsar-core"

-- TODO: Do not force cpulsar to be linked dynamically
-- cpulsar needs to be built by a C++ compiler.
-- However, you can include its headers and link with it in C
project "cpulsar"
  kind "SharedLib"
  language "C++"
  cppdialect "C++20"

  defines "CPULSAR_SHAREDLIB"

  buildpath.setup("cpulsar")

  includedirs "../include"
  files {
    "../src/cpulsar/**.cpp", "../include/cpulsar/**.h",
    "../src/pulsar/**.cpp", "../include/pulsar/**.h"
  }
  removefiles { "../src/pulsar/core.cpp" }
  -- Needs to dynamically link with pulsar-core so that it can be used as a shared library.
  -- pulsar-core must be built by whatever program tries to load cpulsar (or the shared library linked with cpulsar)
  links { "pulsar-core" }

  cflags()

  pic "On"

  filter "system:linux"
    defines "CPULSAR_UNIX"

  filter "system:windows"
    defines "CPULSAR_WINDOWS"
