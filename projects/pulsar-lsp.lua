require("premake", ">=5.0.0-beta4")

local buildpath = require "common/buildpath"
local cflags    = require "common/cflags"

include "pulsar"
include "../libs/lsp-framework/lspframework"

project "pulsar-lsp"
  kind "ConsoleApp"
  language "C++"
  cppdialect "C++20"

  dependson "lspgen"

  buildpath.setup("pulsar-lsp")

  includedirs {
    "../include",
    "../libs/lsp-framework",
    "../libs/lsp-framework/build/lspframework/generated"
  }
  files { "../src/pulsar-lsp/**.cpp", "../include/pulsar-lsp/**.h" }
  links { "pulsar", "lspframework" }

  cflags()

  filter { "configurations:Debug*", "options:lsp-use-sanitizers" }
    buildoptions { "-fsanitize=address,undefined", "-fno-omit-frame-pointer" }
    linkoptions  { "-fsanitize=address,undefined" }
