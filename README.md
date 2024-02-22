# Pulsar

*A small stack-based dynamically-typed interpreted scripting language.*

> The main purpose of this language is to be an alternative to CSS
in an highly anticipated upcoming JavaScript Framework,
which will revolutionize the Web as we know it
by exposing the, still untouched, performance of modern web browsers.

Not really. It's just another side-project of mine.
I wanted to have a second attempt at creating a programming language.

## Language Documentation

It can be found within the [docs](docs) folder.

## Editor Extensions

If you want to enable Syntax Highlighting on VSCode for the Pulsar Programming
Language, you can download and package the official extension found at
[Marco4413/vscode-pulsar-language](https://github.com/Marco4413/vscode-pulsar-language).

## Dependencies

**None!** Except for the `pulsar-dev` project which uses `fmt` for pretty printing.

The Language itself has no dependencies except for the C++20 standard library.

## Building

This project uses `premake5` as its build system.

Run `premake5 vs2022` to create solution files for VS.

Alternatively `premake5 gmake2` will create Makefiles.

C++20 is the standard used by the project.
Supported compilers are `gcc` and `msvc`.

## Examples

Check out the [examples](examples).
