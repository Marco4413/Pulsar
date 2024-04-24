term.pushColor(term.yellow)
print("WARNING: IF YOU SEE THIS MESSAGE AND YOU ARE TRYING TO INCLUDE THE PULSAR PROJECT, YOU ARE ACTUALLY INCLUDING THE WORKSPACE FILE.")
print("      -> If so, make sure to include the pulsar folder instead.")
term.popColor()

workspace "pulsar"
   -- architecture "x64"
   configurations { "Debug", "Release" }
   startproject "pulsar-tools"

include "pulsar"
include "libs/fmt"

project "pulsar-demo"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   location "build/pulsar-demo"
   targetdir "%{prj.location}/%{cfg.buildcfg}"

   -- fmt is not needed by Pulsar, it's only used by pulsar-demo for logging
   includedirs { "include", "libs/fmt/include" }
   files "src/pulsar-demo/main.cpp"
   links { "pulsar", "fmt" }

   -- You can even tell Pulsar to not use the filesystem C++ header.
   -- Though it is used by pulsar-demo, so it would not work.
   -- defines "PULSAR_NO_FILESYSTEM"

   filter "toolset:gcc"
      buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-Werror" }

   filter "action:vs*"
      flags { "FatalWarnings" }
      warnings "Extra"
      externalwarnings "Extra"

   filter "toolset:msc"
      buildoptions { "/W4", "/WX" }

   filter "configurations:Debug"
      defines "PULSAR_DEBUG"
      symbols "On"

   filter "configurations:Release"
      optimize "Speed"

project "pulsar-tools"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   location "build/pulsar-tools"
   targetdir "%{prj.location}/%{cfg.buildcfg}"

   includedirs { "include", "libs/fmt/include" }
   files { "src/pulsar-tools/**.cpp", "include/pulsar-tools/**.h" }
   links { "pulsar", "fmt" }

   filter "toolset:gcc"
      buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-Werror" }

   filter "action:vs*"
      flags { "FatalWarnings" }
      warnings "Extra"
      externalwarnings "Extra"

   filter "toolset:msc"
      buildoptions { "/W4", "/WX" }

   filter "system:linux"
      links "pthread"

   filter "configurations:Debug"
      defines "PULSAR_DEBUG"
      symbols "On"

   filter "configurations:Release"
      optimize "Speed"
