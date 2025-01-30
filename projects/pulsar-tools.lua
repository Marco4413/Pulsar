require("premake", ">=5.0.0-beta4")

local buildpath = require "common/buildpath"
local cflags    = require "common/cflags"

local function get_slib_filename(basename)
  if os.target() == "windows" then
    return basename .. ".dll"
  else
    return "lib" .. basename .. ".so"
  end
end

local function get_slib_path(projectName)
  return buildpath.of(projectName) .. "/" .. get_slib_filename(projectName)
end

local function copy_slib(of, to)
  return "{COPYFILE} %[" .. get_slib_path(of) .. "] %[" .. buildpath.of(to) .. "/" .. get_slib_filename(of) .. "]"
end

include "pulsar"
include "../libs/fmt/fmt"

project "pulsar-tools"
  kind "ConsoleApp"
  language "C++"
  cppdialect "C++20"

  buildpath.setup("pulsar-tools")

  buildinputs {
    get_slib_path("cpulsar"),
    get_slib_path("pulsar-core")
  }

  postbuildcommands {
    copy_slib("cpulsar", "pulsar-tools"),
    copy_slib("pulsar-core", "pulsar-tools")
  }

  buildoutputs {
    buildpath.of("pulsar-tools") .. "/" .. get_slib_filename("cpulsar"),
    buildpath.of("pulsar-tools") .. "/" .. get_slib_filename("pulsar-core")
  }

  includedirs { "../include", "../libs/fmt/include", "../libs/argue" }
  files {
    "../src/pulsar-tools/**.cpp", "../include/pulsar-tools/**.h",
    "../libs/argue/argue.hpp"
  }
  links { "pulsar", "fmt" }

  cflags()

  filter "system:linux"
    defines "PULSARTOOLS_UNIX"

  filter "system:windows"
    defines "PULSARTOOLS_WINDOWS"
