# Pulsar-Tools Native Bindings

Here you'll find a full list of all available bindings from Pulsar-Tools!

> *This document implies that you have read the [Language Documentation](LANGUAGE.md).*

## Debug

As you may imagine this is a collection of debugging utilities.

### stack-dump!

`*(*stack-dump!).`

Prints the whole stack of the calling function.

### trace-call!

`*(*trace-call!).`

Prints the entire call stack up to the native call.

## FS

The FileSystem API.

See [examples/basic/10-file_system](../examples/basic/10-file_system.pls)

### fs/exists?

`*(*fs/exists? path) -> 2.`

Types: `String -> String, Integer`

Returns `path` and a non-zero value if `path` exists.

### fs/read-all

`*(*fs/read-all path) -> 1.`

Types: `String -> String`

Returns the contents of the file at `path`.

Errors if `path` is not a file.

## Lexer

Bindings to the Pulsar Lexer.

> *Don't ask me why they even exist.*

See [examples/advanced/00-lexer_bindings](../examples/advanced/00-lexer_bindings.pls)

### lexer/from-file

`*(*lexer/from-file path) -> 1.`

Types: `String -> Lexer`

Creates a new `Lexer` from the file at `path`.

If the file does not exist returns an invalid `Lexer`.
Use [`(*lexer/valid?)`](#lexervalid) to check if no error occurred.

Don't forget to call [`(*lexer/free!)`](#lexerfree) when you're done!

### lexer/next-token

`*(*lexer/next-token lexer) -> 1.`

Types: `Lexer -> [ Integer, String, Any? ]`

Returns the next token of `lexer` as a `List`.

The `List` representing the token contains an `Integer` representing the
Token's Type, a `String` which is the name of the Token and an optional
`Any` value which is the literal value.

### lexer/free!

`*(*lexer/free! lexer).`

Types: `Lexer ->`

Deletes `lexer` from memory.

### lexer/valid?

`*(*lexer/valid? lexer).`

Types: `Lexer ->`

Checks if `lexer` is valid (was created and should be freed).

## Module

Bindings to programmatically execute other Pulsar files.

> *Still WIP/not really used yet.
They won't be documented until they're stable.*

### module/from-file

### module/run

### module/free!

### module/valid?

## Panic

These methods will allow you to terminate execution to prevent
undefined behaviour of functions.

See [examples/intermediate/04-type_checking](../examples/intermediate/04-type_checking.pls)

### panic!

`*(*panic!).`

Throws `RuntimeState::Error`.

Pops its call from the call stack before throwing the error.
That way the error is shown to be within the caller.

### panic/type!

`*(*panic/type!).`

Throws `RuntimeState::TypeError`.

Pops its call from the call stack before throwing the error.
That way the error is shown to be within the caller.

## Print

Generic functions for printing any value.

### hello-from-cpp!

`*(*hello-from-cpp!).`

C++ says Hi!

### print!

`*(*print! val).`

Types: `Any ->`

Prints **any** value to stdout.

### println!

`*(*println! val).`

Types: `Any ->`

Prints **any** value to stdout and adds a new line.

## STDIO

Direct access to `stdin` and `stdout` for `String` I/O.

### stdin/read

`*(*stdin/read) -> 1.`

Types: `-> String`

Reads one line from `stdin` and returns it (without the new line character).

### stdout/write!

`*(*stdout/write! str).`

Types: `String ->`

Writes `str` to `stdout`.

### stdout/writeln!

`*(*stdout/writeln! str).`

Types: `String ->`

Writes `str` and a new line character to `stdout`.

## Thread

Threads within Pulsar!

Pulsar is not actually designed to be Thread-Safe.
Meaning that it's very limited in what it can do.

The only custom type that can be passed to a thread is the
[`Channel`](#channelnew) type, other types (from this document,
even `Thread`s) lose their reference for thread-safety reasons.

The [`Channel`](#channelnew) type allows to send/receive data to/from
running threads.

See [examples/advanced/02-thread_bindings](../examples/advanced/02-thread_bindings.pls)

See [examples/advanced/03-thread_channels](../examples/advanced/03-thread_channels.pls)

### thread/run

`*(*thread/run args fn) -> 1.`

Types: `List, FunctionReference -> Thread`

Runs a new thread with `fn` and `args`. All Globals are copied from
the current context.

All `args` are pushed onto the stack before calling `fn`.

### thread/join

`*(*thread/join thread) -> 2.`

Types: `Thread -> List, Integer`

Waits for `thread` to complete and returns a `List` containing its return values
and the `RuntimeState` as an `Integer` (no error if 0).

### thread/join-all

`*(*thread/join threads) -> 1.`

Types: `[ ...Thread ] -> [ ...[ Integer, List ] ]`

Waits for each `Thread` in `threads` to complete and returns a `List` containing
all `RuntimeState`s and return values of the `Thread`s.

### thread/alive?

`*(*thread/alive? thread) -> 2.`

Types: `Thread -> Integer`

Given a `thread` returns an `Integer` which is 0 if the `thread`
is not running. Meaning that joining `thread` does not block.

### thread/valid?

`*(*thread/valid? thread) -> 2.`

Types: `Thread -> Integer`

Given a `thread` returns an `Integer` which is 0 if the `thread`
is not valid (does not exist or was joined).

### channel/new

`*(*channel/new) -> 1.`

Types: `-> Channel`

Creates a new open `Channel`.

`Channel`s can be passed to `Thread`s!

### channel/send!

`*(*channel/send! value channel).`

Types: `Any, Channel ->`

Puts `value` into `channel` so that a call to `(*channel/receive)`
on `channel` returns `value`.

Errors if `channel` is closed.

### channel/receive

`*(*channel/receive channel) -> 1.`

Types: `Channel -> Any`

Blocks until either `channel` has some value or it's closed.
If `channel` is closed and empty, type `Void` is returned.

### channel/close!

`*(*channel/close! channel).`

Types: `Channel ->`

Marks `channel` as closed.
- Calls to [`(*channel/send!)`](#channelsend) will error.
- Calls to [`(*channel/receive)`](#channelreceive) will return `Void` if `channel` is empty.

### channel/free!

`*(*channel/free! channel).`

Types: `Channel ->`

Closes `channel` and deletes references to it.
- Blocked calls to [`(*channel/receive)`](#channelreceive) will act the same as [`(*channel/close!)`](#channelclose).

### channel/empty?

`*(*channel/empty? channel) -> 1.`

Types: `Channel -> Integer`

Returns a non-zero `Integer` if `channel` is empty.

If not empty, calls to [`(*channel/receive)`](#channelreceive) won't block.
Though if other `Thread`s consume values between the check and the receive
call it may block.

### channel/closed?

`*(*channel/closed? channel) -> 1.`

Types: `Channel -> Integer`

Returns a non-zero `Integer` if `channel` is closed.

### channel/valid?

`*(*channel/valid? channel) -> 1.`

Types: `Channel -> Integer`

Returns a non-zero `Integer` if references `channel` still exist.
Meaning [`(*channel/free!)`](#channelfree) was not yet called on `channel`.

## Time

Functions to keep track of time!

See [examples/basic/11-time](../examples/basic/11-time.pls)

### time

`*(*time) -> 1.`

Types: `-> Integer`

Returns the [UNIX Time](https://en.wikipedia.org/wiki/Unix_time) in milliseconds.

### time/steady

`*(*time/steady) -> 1.`

Types: `-> Integer`

Returns the current time in milliseconds as seen by the Monotonic Clock of the system.

### time/micros

`*(*time/micros) -> 1.`

Types: `-> Integer`

Returns the current time in microseconds.
