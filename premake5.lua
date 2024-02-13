term.pushColor(term.yellow)
print("WARNING: IF YOU SEE THIS MESSAGE AND YOU ARE TRYING TO INCLUDE THE PULSAR PROJECT, YOU ARE ACTUALLY INCLUDING THE WORKSPACE FILE.")
print("      -> If so, make sure to include the pulsar folder instead.")
term.popColor()

workspace "pulsar"
   -- architecture "x64"
   configurations { "Debug", "Release" }
   startproject "pulsar-dev"

include "pulsar"

project "pulsar-dev"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   location "build"
   targetdir "%{prj.location}/%{cfg.buildcfg}"

   includedirs "include"
   files "src/main.cpp"
   links "pulsar"

   filter "toolset:gcc"
      buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-Werror" }

   filter "toolset:msc"
      buildoptions { "/Wall", "/WX" }

   filter "configurations:Debug"
      defines "PULSAR_DEBUG"
      symbols "On"

   filter "configurations:Release"
      optimize "Speed"
