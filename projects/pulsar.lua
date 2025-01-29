require("premake", ">=5.0.0-beta4")

local buildpath = require "common/buildpath"
local cflags    = require "common/cflags"

project "pulsar"
  kind "StaticLib"
  language "C++"
  cppdialect "C++20"

  buildpath.setup("pulsar")

  includedirs "../include"
  files { "../src/pulsar/**.cpp", "../include/pulsar/**.h" }

  -- Define PULSAR_NO_FILESYSTEM to disable the usage of the FileSystem API.
  --   defines "PULSAR_NO_FILESYSTEM"
  -- You can also define PULSAR_NO_ATOMIC to disable the usage of the Atomic header.
  -- Atomics are mainly used by Pulsar::SharedRef to support multi-threading.
  --   defines "PULSAR_NO_ATOMIC"

  cflags()
