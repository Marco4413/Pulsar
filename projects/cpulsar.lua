require("premake", ">=5.0.0-beta4")

local buildpath = require "common/buildpath"
local cflags    = require "common/cflags"

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
    "../src/pulsar/**.cpp", "../include/pulsar/**.h",
    "../src/cpulsar/**.cpp", "../include/cpulsar/**.h"
  }

  cflags()

  pic "On"
