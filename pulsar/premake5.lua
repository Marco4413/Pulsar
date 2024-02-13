project "pulsar"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   location "../build"
   targetdir "%{prj.location}/%{cfg.buildcfg}"

   includedirs "../include"
   files { "../src/**.cpp", "../include/**.h" }
   removefiles "../src/main.cpp"

   filter "toolset:gcc"
      buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-Werror" }

   filter "toolset:msc"
      buildoptions { "/Wall", "/WX" }

   filter "configurations:Debug"
      defines "PULSAR_DEBUG"
      symbols "On"

   filter "configurations:Release"
      optimize "Speed"
