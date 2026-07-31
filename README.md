# Pulsar

![logo](logo.png)

*A small
[concatenative](https://en.wikipedia.org/wiki/Concatenative_programming_language)
[stack-based](https://en.wikipedia.org/wiki/Stack-oriented_programming)
[dynamically-typed](https://en.wikipedia.org/wiki/Type_system#Dynamic_type_checking_and_runtime_type_information)
[interpreted](https://en.wikipedia.org/wiki/Interpreter_(computing))
[scripting language](https://en.wikipedia.org/wiki/Scripting_language).*

> The main purpose of this language is to be an alternative to CSS
in an highly anticipated upcoming JavaScript Framework,
which will revolutionize the Web as we know it
by exposing the, still untouched, performance of modern web browsers.

Not really. It's just another side-project of mine.
I wanted to have a second attempt at creating a programming language.

## Language Documentation

It can be found at [docs/LANGUAGE](docs/LANGUAGE.md).

## Editor Extensions

If you want to enable Syntax Highlighting and add support for the LSP Server
on VSCode for the Pulsar Language, you can download and package the official extension found at
[Marco4413/vscode-pulsar-language](https://github.com/Marco4413/vscode-pulsar-language).

## Dependencies

> All dependencies are git submodules which reference forks/repos owned by
> [Marco4413](https://github.com/Marco4413) of the projects specified here.

- Pulsar:
  - **None!**
- Pulsar-Bindings:
  - **None!**
- Pulsar-Demo:
  - **None!**
- Pulsar-LSP:
  - [`lsp-framework`](https://github.com/leon-bckl/lsp-framework)
- Pulsar-Tools:
  - [`Argue`](https://github.com/Marco4413/Argue)

## Building

This project uses `premake5` as its build system.

Run `premake5 --arch=x86_64 vs2022` to create solution files for VS.

**NOTE:** Run `premake5 --help` to get a list of available architectures.
I sometimes test Pulsar on *arm64* so it should also compile to that just fine.

Alternatively `premake5 gmake2` will create Makefiles.

C++20 is the standard used by the project.
Supported compilers are `gcc` and `msvc`.

**Compiler Versions:**
- clang 18.1.3
- gcc 13.3.0
- msvc from vs2022

*I usually test builds with `gcc`, builds with other compilers may
break between commits. See the Build GitHub Action to check which
compilers work.*

[![Build](https://github.com/Marco4413/Pulsar/actions/workflows/build.yaml/badge.svg)](https://github.com/Marco4413/Pulsar/actions/workflows/build.yaml)

### Building Pulsar-LSP

`pulsar-lsp` should have no issues building. However, if you're using a
C++ LSP Server on your editor, you may notice that some header files are missing.
That happens because `lsp-framework` generates some header files from a json file,
which means that you must build `lsp-framework` before editing files from `pulsar-lsp`.

Building with Make:
```sh
$ make -j lspframework
```

On Visual Studio use the solution explorer to build the `lspframework` project.

## Examples

They're within the [examples](examples) folder.

### Running Examples

The new `pulsar-tools` project is the preferred way of running examples.
In fact, it's the CLI tool for Pulsar.

You can obtain `pulsar-tools` with these methods:
- [Building it yourself](#building).
- From the [Build GitHub Action](https://github.com/Marco4413/Pulsar/actions/workflows/build.yaml) artifacts.
- From [GitHub Releases](https://github.com/Marco4413/Pulsar/releases).

After obtaining `pulsar-tools`, you'll be able to run it with no arguments to
print its usage. The default settings are the ones used for the examples.

```sh
$ pulsar-tools run -- path/to/pulsar/file.pls scriptArg1 scriptArg2
```

### Including Pulsar in your Project

There's a fully working demo within the `pulsar-demo` project.
You can check out the `projects/pulsar-demo.lua` and `projects/common/cflags.lua`
scripts and the source code within `src/pulsar-demo`

### Creating C Native Bindings

Use the [`raylib-pulsar`](https://github.com/Marco4413/raylib-pulsar) project
as an example.
