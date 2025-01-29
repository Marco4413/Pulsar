return function()
  filter "toolset:clang"
    buildoptions {
      "-Wall", "-Wextra", "-Wpedantic", "-Werror",
      "-Wno-gnu-zero-variadic-macro-arguments"
    }

  filter "toolset:gcc"
    buildoptions { "-Wall", "-Wextra", "-Wpedantic", "-Werror" }

  filter "action:vs*"
    fatalwarnings { "All" }
    warnings "Extra"
    externalwarnings "Extra"

  filter "toolset:msc"
    buildoptions { "/W4", "/WX" }

  filter "configurations:Debug*"
    defines "PULSAR_DEBUG"
    symbols "On"

  filter "configurations:Release*"
    optimize "Speed"
end
