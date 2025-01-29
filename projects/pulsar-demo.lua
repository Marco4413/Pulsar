require("premake", ">=5.0.0-beta4")

local buildpath = require "common/buildpath"
local cflags    = require "common/cflags"

include "pulsar"
include "../libs/fmt/fmt"

project "pulsar-demo"
  kind "ConsoleApp"
  language "C++"
  cppdialect "C++20"

  buildpath.setup("pulsar-demo")

  -- fmt is not needed by Pulsar, it's only used by pulsar-demo for logging
  includedirs { "../include", "../libs/fmt/include" }
  files "../src/pulsar-demo/main.cpp"
  links { "pulsar", "fmt" }

  -- You can even tell Pulsar to not use the filesystem C++ header.
  -- Though it is used by pulsar-demo, so it would not work.
  --   defines "PULSAR_NO_FILESYSTEM"
  -- Moreover, you can disable the usage of atomics if you don't plan
  -- to use multi-threading (as of now, they're only used by SharedRef).
  --   defines "PULSAR_NO_ATOMIC"

  cflags()
