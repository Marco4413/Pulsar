# The Pulsar Language Documentation

```lisp
*(*println! val).

*(list-println! list):
  <- list (!empty?) if: .
  (!head) (*println!)
  (list-println!)
  .

*(main args):
  [ // ASCII art generated with https://marco4413.github.io/MediaAsciifier/
    "       3",
    "     -44444454",
    "    !44331  2445555:",
    "   343  33c       45",
    "  541    22",
    ",55c     =21  44443",
    "=5.       ?1!_22222",
    "           11111111",
    "            100000?",
    "            :?????b",
    ";33221       ?!!!!_       ;22334",
    "43221100??.   ?!!!    b??01122333",
    " !22110??!!!!+_!!  a!!!!?001123-",
    "       b?!!!!!!!!!!!!!!!?-",
    "              !!!!a",
    "           =!!!, !!!!",
    "         :???!;  _!!???",
    "        11000a    b?0001?",
    "      a22211a      0111222",
    "     443332,        1223334a",
    "     04443          +134444,",
    "        b            ?23:      ,5-",
    "                      23=     a55.",
    "                       33    255",
    "              53       b43  553",
    "              ;5555542  34455!",
    "                    55555555,",
    "                          4",
    "       Welcome to Pulsar!",
  ] (list-println!)
  .
```

## It's stack-based!

An important part of the Pulsar scripting language is its concatenative and stack-based nature.

Constants and locals, also referred to as `lvalue`s, are pushed onto it all the time.

```lisp
*(do-sum a b) -> 1:
  // The values taken from the caller are bound to the locals
  //   'a' and 'b', a special kind of local called argument.
  // 'a' and 'b' are pushed onto the stack by their name.
  // Then the + operator pops the two values and outputs their sum.
  a b +
  // The . operator is the equivalent of the return statement.
  .

*(main args) -> 1:
  // The constants 2 and 3 are pushed onto the stack.
  // Then (do-sum) is called and consumes those two values
  //   because it accepts two arguments.
  2 3 (do-sum)
  .
```

## Functions, functions and more functions.

A Pulsar file contains function definitions.
All code must be contained within a function.

A cool feature of this language is its "infinite" recursion capabilities.
Unless you run out of RAM first! No recursion is used internally at
runtime to call functions.

---

Now that these premises (and examples) *should* have helped you get the
gist of Pulsar, we can get to the meat of it.

## Comments

Three types of comments exist.

The multi-line comment `/* COMMENT */` which can be used to comment out
multiple lines.

The line comment `//` which comments out an entire line.

The character comment `;` which can be used to improve code readability
by *visually* separating *logical* expressions.

i.e. `10 -> x; x` is more readable than `10 -> x x`.

## Sha-Bang

For anyone who's not familiar with the Unix standard of specifying
interpreters for text files, here's the [Wikipedia page](https://en.wikipedia.org/wiki/Shebang_%28Unix%29).

Sha-bang(s) may only appear at the top of a source file and are
treated like a line [comment](#comments).

Of course it's platform independent, so it's only QoL for Unix
users. Which means it doesn't hurt to place it at the top of your
source files even if you're only planning to use the script on
Windows.

```sh
#!/usr/bin/env pulsar-tools
```

The above example of a sha-bang expects the `pulsar-tools` executable
to be in the `PATH`. If so, it will run the file it's in with the
`pulsar-tools` interpreter.

## Identifiers

Identifiers can start with any letter from the alphabeth or the underscore character.
Then can continue with any starting character or digit and the following symbols:
`<=>?+-*/!`.

At the moment, identifiers cannot be the same as keywords.

## Compiler Directives

Compiler Directives are special statements that are executed by the compiler (it's
actually the parser, compiler sounds cooler though).

A directive is composed by an identifier preceded by the `#` character (e.g.
`#mydirective`). Some may accept arguments. THEY ARE CASE SENSITIVE.

### \#include

`#include "path/to/my/file.pls"`

The `include` directive allows you to specify another file which will be **parsed
as if it was copy-pasted** where the directive was used.

The parser won't allow a file to be parsed more than once, it will just ignore the
other directive. So you don't have to worry about what other files include.

The path to the file is relative to the including file.

## Datatypes

Pulsar is dynamically-typed, any value can be assigned to any local.
And of course all values can be pushed onto the stack.

Though some type checking can be done by operators/statements.
And a runtime error may be raised. No error catching is supported
within the language itself.

### Basic Datatypes

|          Type           | Bit Count |
| :---------------------: | :-------: |
|         Integer         |    64     |
|         Double          |    64     |
|    FunctionReference    |    64     |
| NativeFunctionReference |    64     |
|         Custom          |    192    |

> ~~Support for unsigned integers is planned.~~
>
> This message has been here for a few months now:
> It's probably not happening.

The Custom datatype is mainly intended to be returned by [Native Functions](#native-functions).
It holds the Id of the Type and a Shared Reference to its data.
While all values in Pulsar are passed around by value, the Custom
type is the only one which is always passed by reference
(it's still by value, but it holds a reference so you can see it
like that).

#### Integer Literals

There exist 4 types of Integer Literals:

1. The decimal literal (e.g. +40 where '+' is optional, -20)
2. The hex literal (e.g. 0xFF, 0xff)
3. The octal literal (e.g. 0o77)
4. The binary literal (e.g. 0b11)

You should be familiar with all of them.

#### Double Literals

Double Literals MUST start with a digit, they can't start with '.' because it could
be confused with the [return statement](#the-return-statement).

They must have a '.' between two sets of digits to separate the fractional part, otherwise
it would be an [integer literal](#integer-literals).

Currently there's no support for exponential notation (i.e. 0.1E4).

#### CustomTypeData

*CustomTypeData* is a global kind of data which is shared across the
whole execution context. It may be provided by native bindings.

*CustomTypeData* was previously used to hold information about
custom types provided by native bindings. However, the *Custom*
type now holds a direct reference to its data. As of now, it's
no longer used by standard bindings.

### Complex Datatypes

|  Type  |
| :----: |
|  List  |
| String |

`List`s are stored as `LinkedList`s. Therefore, appending, prepending
items or concatenating another `List` to them doesn't cost much
performance-wise. However, copying and calculating their length is slow.

#### String Literals

It's your usual double-quoted string which can contain escape characters.
When a `\` is found within quotes, the next character is parsed as is
except for some special ones which are turned into other characters:

| Escape Sequence |     Character     |
| :-------------: | :---------------: |
|      `\n`       |     New Line      |
|      `\r`       |  Carriage Return  |
|      `\t`       |    Tabulation     |
|    `\xHH;`      |  Char Code 0xHH   |

Where H is an hex digit.

Unicode characters may be present within Strings.
However, the language itself does not handle their encoding (yet).

i.e. The [`index`](#index) instruction will index a single byte within the String.

#### Multi-Line String Literals

Pulsar uses an approach similar to C for multi-line strings.

In C you would write:

```c
  "Hello,"
"\nWorld!"
```

The C compiler automatically concatenates two (or more) string literals
which are next to each other.

However, Pulsar cannot do that without some extra syntax, since that
same code should push two separate strings to the stack.

So here's Pulsar's syntax:

```lisp
  "Hello,"
\n"World!"

 "Hello, "
\"World!"
```

The first one produces `"Hello,\nWorld!"`.

The second one `"Hello, World!"`.

TL;DR You can write `\` or `\n` between two string literals to let the
parser concatenate the two strings (the latter adds a new line between
the two strings).

#### List Literals

List literals are comma-separated *lists* defined between brackets of `lvalue`s.

A valid list literal looks something like this: `[ "Hello", "World", 1, 2, 3, ]`

*Where the last comma is optional.*

A list literal (unless it contains only numeric constants) is broken down into the
[instructions](#instructions) needed to create the specified list. Groups of numeric
constants are produced in one or two instructions depending on the context.

#### "Character Literals"

Character literals aren't really character literals. They're a convenient way of
writing an **Integer Literal that matches the ASCII code of a character**.

They're single-quoted: `'c'`

All escape sequences which produce a single character in a [String Literal](#string-literals)
are also valid in Character Literals.

i.e. `'\''`, `'\xFF;'` are valid.

## Mathematical Operators

| Symbol |   Name  | Args | Returns |
| :----: | :-----: | :--: | :-----: |
|   *    |   Star  |  2   |    1    |
|   +    |   Add   |  2   |    1    |
|   -    |   Sub   |  2   |    1    |
|   /    |  Divide |  2   |    1    |

All these operators are self-explainatory.
They're your usual mathematical operations.
Though you have to keep in mind that we're working with a stack!
So operator priority is not a thing, the last one used is the
last one to be performed!

`Double` is stronger than `Integer`, so `1 1.0 +` turns into `2.0`.

The following ones only operate on `Integer`s:

| Symbol |      Name     | Args | Returns |
| :----: | :-----------: | :--: | :-----: |
|   %    |    Modulus    |  2   |    1    |
|   &    |    BitAnd     |  2   |    1    |
|  \|    |     BitOr     |  2   |    1    |
|   ~    |    BitNot     |  2   |    1    |
|   ^    |    BitXor     |  2   |    1    |
|   <<   | BitShiftLeft  |  2   |    1    |
|   >>   | BitShiftRight |  2   |    1    |

Bit Shifts are logical shifts. Negative shifts swap the direction.

## Comparison Operators

| Symbol |   Name   | Args | Returns |
| :----: | :------: | :--: | :-----: |
|   =    |  Equals  |  2   |    1    |
|   !=   |  NotEq   |  2   |    1    |
|   <    |   Less   |  2   |    1    |
|   <=   | LessOrEq |  2   |    1    |
|   >    |   More   |  2   |    1    |
|   >=   | MoreOrEq |  2   |    1    |

As of now, these are only allowed within [if statements](#if-statement).

However, you can achieve the same behaviour through the [compare](#compare)
and [equals?](#equals) instructions.

= and != work on all [Data Types](#datatypes).

While the others only work on Numeric and String [Data Types](#datatypes).

## Special Operators

Special operators are the ones that do not just consume N values and
push M onto the stack. Like assignment operators or the 'FullStop' (aka Return) operator.
They modify the state of the execution context and/or are parsed differently to other operators.

### The RightArrow Operator

This operator can be used to create your own locals and/or update
values of already-existing locals. It assigns to the specified local
the last value on the stack and pops it.

```lisp
*(func! x y) -> 0:
  // 'x' already exists so its value will be updated.
  10 -> x
  // 'a' doesn't exist so it's created and assigned to
  //  the top-most value on the stack.
  11 -> a
  .
```

Creation of a new local can be forced by adding `!` in front of its
name. This can be useful to create scope-specific locals with the same
name as one that's on its parent scope (shadowing).

```lisp
*(func! x y) -> 0:
  do:
    // 'x' within this do block has the value 10.
    10 -> !x
  end
  // 'x' keeps its original value.
  x -> y
  .
```

> This operator is the reason why constants and locals are referred to
as `lvalue`s, because they usually stand to the left of the operator.

### The LeftArrow Operator

As you might have seen, the standard way of putting a local into the stack
is by using its name. That, however, copies the value of the local into the
stack, which can get slow if doing so on complex data types (e.g. `List`).

Another way of putting a local into the stack is using the `<-` operator.

```lisp
*(swap x y) -> 2:
  // Super fast functional swap!
  <- y <- x
  .
```

This operator **moves** the local into the stack invalidating its value.
Use it if you really want to improve the performance of your program, and
are working with complex data types.

This operator produces an `lvalue`, which means that it can be used in [`List` literals](#list-literals).

### The BothArrows Operator

Another local-management operator is the `<->` operator. While `->` moves
the last value on the stack into a local, `<->` copies the value without
popping it from the stack.

Before the introduction of this operator, you had to write `-> var var`
to do the same thing. Which was "less readable" and efficient.

```lisp
*(main args) -> 1:
  // This expression results in the value '2'.
  1 <-> x x +
  .
```

### The PushReference Operator

The push reference operator is a very powerful operator. It allows you
to push a reference to a function onto the stack, then you can call it
with the [icall](#icall) instruction.

`<& (function)`

`(*native)` functions can also be passed to the operator.

The provided function follows the [Function Call](#function-calls) syntax.

This operator produces an `lvalue`, which means that it can be used in [`List` literals](#list-literals).

## Control Flow Statements

### The Return Statement

The return statement (aka `.`) is used to mark the end of functions or to end them early
(return) if within a block (ifs and loops).

### The Break Statement

The break statement is used to break out of a code block, jumping to the end of it.

It breaks out of **the last skippable block** if supported.

```lisp
*(*println! val).

*(main args):
  0 -> i
  while:
    i if >= 10:
      break
    i (*println!)
    i 1 + -> i
  end
  .
```

### The Continue Statement

The continue statement is used to repeat a code block, which means jumping back to its start.

It repeats **the last skippable block** if supported.

```lisp
*(*println! val).

*(main args):
  0 -> i
  while:
    i (*println!)
    i 1 + -> i
    i if < 10:
      continue
    break
  end
  .
```

## Functions

Functions take some arguments from the caller's stack and return some.

A function definition looks like this:

```lisp
*(func-name K arg1 arg2 ... argN) -> M:
  // Do Stuff
  .
```

Where `K` and `M` are [integer literals](#integer-literals).
`K` and `-> M` are optional and they default to 0.

When the function is called, N+`K` values are taken from the caller's stack.
`K` values are moved into the stack. While N values are bound to locals.

When the function returns, `M` values must be on the stack and they are
moved into the caller's stack.

**Functions with the same name may exist. See below to understand
what function will be called.**

**NOTE: I don't know how to feel about this feature. I may either
add Parser warnings or remove it entirely in the future.**

### Function Calls

Functions are called by `(function)`.

> It's like function definitions but without the `*` in front of
it and the arguments within the parentheses.

**If two functions have the same name, the most-recent one is used.**

### Function Naming

Function names are [identifiers](#identifiers) and they allow the `/`
character to be used after the first character. So if you plan to make
an [\#include](#include)-able file, you should probably use a
"namespaced" name like `my-module/function-name`.

The rest is up to you! You can use `?` for value-checking functions,
`!` for procedures, ...

## Native Functions

Native functions must be declared within a Pulsar source file.
Then they must be bound before running the Pulsar module
within the native implementation.

```lisp
*(*native-func-name K arg1 arg2 ... argN) -> M.
```

Signatures must match so that the right amount of arguments
can be taken and added to the caller's stack.

As always, `K` and `-> M` default to 0.

There's no real reason native functions should have a value for `K`.
Native code is fast, and having some sort of documentation within the name
of the arguments is a better choice.

**Native function can't be redefined with a different signature\*.**

\* You can't have two native functions with the same name and different
argument and return counts.

But you can redefine one if you provide the same signature. Which is
useful when you don't know whether an included file defined it or not.

## If Statement

The if statement comes in a bunch of different flavours!

Before getting into details about it, I'll explain the most
straightforward one.

```lisp
*(main args) -> 1:
  10 -> x
  if x > 8:
    8
  else if x > 6:
    6
  else if x > 4:
    4
  else if x > 2:
    2
  else:
    0
  end
  .
```

It's your typical if statement! You can't go wrong with it.

All [Comparison Operators](#comparison-operators) are supported.
Literals allowed within if conditions are strings and numeric data types.
However, that does not stop you from comparing two values bound to locals.

The `end` keyword must be specified at the end of the last branch.

### More Ifs

The following pattern describes all available Ifs:

`if [not] [[<lvalue>] [<comp> <lvalue>]]:`

> Woah, that's A LOT of `if`s.

First of all, `lvalue`s that the if statement supports are
numeric/string literals and names of locals.

If both `lvalue`s (and `comp`) are present the `if` is
considered as self-contained. Meaning that it does not consume stack
values outside of the condition description (which are the ones shown
in the previous section). And that, for obvious reasons, allows for
safe `else if` chaining. Otherwise, you would not be able to know
how many values are popped from the stack.

If only `<comp> <lvalue>` is present then the comparison is done
with the last value on the stack (and of course it's popped).

Moreover, if only `<lvalue>` is provided then `= <lvalue>` is implied.

Finally, if no condition at all is present, `!= 0` is implied.

The `else` branch can be specified within almost all `if`s.

When an `if` statement is not self-contained a [control flow statement](#control-flow-statements)
may replace the `end` keyword, thus an explicit `else` branch cannot exist.

The `not` keyword may be used to negate the comparison.

## Local Block

The local block is an easy way to [move](#the-rightarrow-operator) stack values into locals with a function arguments-like syntax.

```lisp
local [...identifiers]:
  // Code here
end
```

If no identifier is provided it acts like a new scope, so you'll be able to hide locals within it.
Moreover, it does not have the draw-back of losing the reference to the last [skippable block](#skippable-blocks)
which the [do block](#do-block) has.

The identifier `_` drops the value instead of creating a new local.

Duplicate identifiers are dropped except for the last one in the list.

Locals defined outside of the local block are shadowed.

**What does "function arguments-like syntax" mean?**

Here's an example:

```lisp
1 2
  // 2 is the top-most value, so "two" must be defined first.
  // To mimic the behaviour of the local block, the binding is forced (-> !).
  -> !two
  -> !one
```

```lisp
1 2 local one two:
  // The function-like syntax allows to define them in an intuitive order.
end
```

Both snippets produce the same VM code.

## Skippable Blocks

### While Loop

The while loop is the one you know and love from other programming languages:

```lisp
*(*println! val).

*(main args):
  0 -> i
  while i < 10:   // Loop while i < 10
    i (*println!) // Print i
    i 1 + -> i    // Increment i
  end
  .
```

It behaves the same way as you'd expect.

All [comparison operators](#comparison-operators) are supported as well as all
datatypes from the [if statement](#if-statement) and the `not` keyword.

If no condition and only an lvalue is provided then it's the same as writing
`while <lvalue> != 0`

If the condition is empty it behaves the same way as `while 1` (a while true loop).

You can disable a while true loop by `while not` or `while 0`.

All [Control Flow Statements](#control-flow-statements) are supported.

The `end` keyword marks the end of the loop.

### Do Block

The do block is a special skippable block. It can be used to create custom loops
or generate a new scope to hide variables from the other ones.

`continue` loops back to the start:
```lisp
*(*println! val).

*(main args):
  // Has the same behaviour as 'do { ... } while(i < 10)'
  0 -> i do:
    i (*println!)
    i 1 + -> i
    i if < 10:
      continue
  end
  .
```

`break` ends the block early:
```lisp
*(*println! val).

*(to-odd n) -> 1:
  n do:
    n 2 % if 1:
      break
    1 +
  end
  .

*(main args):
  0 -> i do:
    i (to-odd) (*println!)
    i 1 + -> i
    i if < 10:
      continue
  end
  .
```

## Globals

Globals can be useful to store some state between functions,
**pre-compute** values during parsing and as a configuration.

```lisp
global const [0, 1, 2, 3] -> my-global
global my-global -> my-global2
```

Global variables are defined as follows:

`global [const] [lvalue] -> name`

**Redefinition of const globals is not allowed.**

**Redefinition of non-const globals will change the previous value.**

The last definition of a global dictates its **runtime** value.

When using the value of a global in another global's definition,
its value is the one of the latest redefinition up until its usage:
```lisp
global 1 -> a
global a -> b
global 2 -> a
// b has the value 1
```

They're treated like locals within functions, so you can copy,
move or set them like you would normally do with locals. However,
if a global is defined as `const` you'll only be able to copy it.

Though as you might have noticed, there's that '**pre-compute**'
word that's VERY important. And now we'll get to why.

### Global producers!

```lisp
global [const] -> name:
  // Do Function Stuff
  .
```

As seen in the [globals example](../examples/intermediate/02-globals.pls), you can do
performance-heavy tasks during parsing. That means increasing compile
time to improve runtime efficiency!

That example goes from ~2.3ms of runtime to ~54us just by pre-computing
the lists it uses.

## Labels

**NOTE: Labels should never be used unless strictly needed. That's the reason
[skippable blocks](#skippable-blocks) exist. They're just more readable. However,
labels were added to be able to decompile any Neutron file into Pulsar code.**

**By default Labels are disabled when running files with `pulsar-tools`.
Check its usage to find the option which enables them.**

A label represents a fixed point within a function body to which you can jump
to using [jump instructions](#jump-instructions). They can also be used in
[global producers](#global-producers).

They are [identifiers](#identifiers) preceded by a `@` symbol.

Any label can be used from any part of the function.
A jump can point to some label which is declared after its usage.

Here's a function which generates a list of `n` numbers from 0 to `n`:

```lisp
*(gen n) -> 1:
  [] 0 -> i while i < n:
    i (!append)
    i 1 + -> i
  end
  .
```

The same function using labels looks like this:

```lisp
*(gen n) -> 1:
  [] 0 -> i
@while
  i n (!compare)
    (!jgez! @endwhile)
  i (!append)
  i 1 + -> i
  (!j! @while)
@endwhile
  .
```

Which is actually what the first snippet compiles to.
Using labels removes all kind of scoping which Pulsar does with blocks.

As stated at the start of this section, labels are useful for decompiling
Neutron files into valid Pulsar code. They're bad for actual code readability.

However, there are some use-cases. Like code which is executed each time on return:

```lisp
// Bindings provided by Pulsar-Tools
// This example may get outdated over time.
// You just have to get the concept out of it.
*(*channel/new) -> 1.
*(*channel/close! ch).

*(func):
  (*channel/new) -> channel
  // ...
    // Somewhere within code
    //  where you want to return.
    (!j! @return)
  // ...
@return
  channel (*channel/close!)
  .
```

## Instructions

You can see instructions as *blazing fast* functions!

They are called with `(!instr arg0)`.

Moreover, they map directly to VM instructions. So they don't even have the
overhead of creating a new Frame and Stack.

Internal arguments MUST be Integer Literals for non-jump instructions and
they're completely optional. Most of the time, the instruction doesn't even
need arguments to do its job.

Meanwhile, you must provide a declared [@label](#labels) to jump instructions.
It is required, and you can't use integers for security reasons (you may be
able to jump in the middle of a [list literal](#list-literals) representation,
which is not cool at all from where I'm from).

### pop

Pops the last value on the stack.

Accepts `arg0`: if <= 1, 1 value is popped. If > 1, `arg0` values are popped.

### swap

Swaps the last two values on the stack.

Let `T` and `U` be any type:

|        | S-1 | S0  |
| :----- | :-: | :-: |
| Pops   | `T` | `U` |
| Pushes | `U` | `T` |

### dup

Duplicates the last value on the stack.

Accepts `arg0`: if <= 1, 1 copy is pushed. If > 1, `arg0` copies are pushed.

|        |  S0   |   S+1   |
| :----- | :---: | :-----: |
| Pops   |  `T`  |         |
| Pushes |  `T`  |  `...T` |

### floor

Truncates the last number on the stack converting it to an `Integer`.

|        |         S0         |
| :----- | :----------------: |
| Pops   | `Integer`/`Double` |
| Pushes |     `Integer`      |

### ceil

Adds 1 and truncates the last number on the stack converting it to an `Integer`.

|        |         S0         |
| :----- | :----------------: |
| Pops   | `Integer`/`Double` |
| Pushes |     `Integer`      |

### compare

Compares the last two elements on the stack,
pushing a value < 0 if a < b, 0 if a = b, > 0 if a > b.

Let `Numeric` be `Integer` or `Double`.

If both values are either `Numeric` it acts like the [*Sub operator*](#mathematical-operators).
meaning that the returned value may be either an `Integer` or `Double`.

If both values are `String`s then the returned value is an `Integer`.
`String` comparison can be used to check for `String` equality or to sort them alphabetically.

|        |        S-1         |         S0         |
| :----- | :----------------: | :----------------: |
| Pops   | `Numeric`/`String` | `Numeric`/`String` |
| Pushes |     `Numeric`      |                    |

### equals?

Checks if the last two elements on the stack are equal,
pushing a non-zero value only if both are the same value.

|        |    S-1    |  S0   |
| :----- | :-------: | :---: |
| Pops   |   `Any`   | `Any` |
| Pushes | `Integer` |       |

### icall

The `icall` instruction pops one value from the stack, checks
if it's a `Native/FunctionReference` (raising an error otherwise)
and calls the function pointed by the reference. Of course, the called
function is called like a normal `(function)` call.

See the [references example](../examples/basic/05-references.pls).

|        |        S-1        |             S0             |
| :----- | :---------------: | :------------------------: |
| Pops   | `...Args`         | `Native/FunctionReference` |
| Pushes | `...ReturnValues` |                            |

### length

Calculates the length of a `List` or `String` and puts it into the stack.

|        |       S0        |    S+1    |
| :----- | :-------------: | :-------: |
| Pops   | `List`/`String` |           |
| Pushes | `List`/`String` | `Integer` |

### empty?

Checks if a `List` or `String` is empty and pushes the boolean value into the stack.

Checking for empty on big lists is faster than checking for their length.

```lisp
<- my-list
  (!empty?) if:
    // Do stuff if empty
  else:
    // Do stuff if not empty
  end
```

|        |       S0        |    S+1    |
| :----- | :-------------: | :-------: |
| Pops   | `List`/`String` |           |
| Pushes | `List`/`String` | `Integer` |

### empty-list

Pushes a new empty `List` into the stack.

|        |  S+1   |
| :----- | :----: |
| Pops   |        |
| Pushes | `List` |

### prepend

Adds a value to the start of a `List` or `String`.

Only `Integer`s or `String`s can be prepended to `String`s.
`Integer`s are converted to ASCII characters.

|        |       S-1       |  S0   |
| :----- | :-------------: | :---: |
| Pops   | `List`/`String` | `Any` |
| Pushes | `List`/`String` |       |

### append

Adds a value to the end of a `List` or `String`.

Only `Integer`s or `String`s can be appended to `String`s.
`Integer`s are converted to ASCII characters.

|        |       S-1       |  S0   |
| :----- | :-------------: | :---: |
| Pops   | `List`/`String` | `Any` |
| Pushes | `List`/`String` |       |

### concat

Concatenates two `List`s.

|        |  S-1   |   S0   |
| :----- | :----: | :----: |
| Pops   | `List` | `List` |
| Pushes | `List` |        |

### head

Removes the first value of a `List` and pushes it into the stack.

|        |   S0   |  S+1  |
| :----- | :----: | :---: |
| Pops   | `List` |       |
| Pushes | `List` | `Any` |

### tail

Removes the first value of a `List`.

|        |   S0   |
| :----- | :----: |
| Pops   | `List` |
| Pushes | `List` |

### index

Copies the value at a specific index of a `String` or a `List`.

`String` characters are converted to `Integer`s.

|        |       S-1       |    S0     |
| :----- | :-------------: | :-------: |
| Pops   | `List`/`String` | `Integer` |
| Pushes | `List`/`String` |   `Any`   |

### prefix

Given a `String` and an `Integer` on the stack, takes the **first**
N characters of the `String` and pushes the result to the Stack.
Removes the new `String` from the old one.

|        |   S-1    |    S0     |
| :----- | :------: | :-------: |
| Pops   | `String` | `Integer` |
| Pushes | `String` | `String`  |

### suffix

Given a `String` and an `Integer` on the stack, takes the **last**
N characters of the `String` and pushes the result to the Stack.
Removes the new `String` from the old one.

|        |   S-1    |    S0     |
| :----- | :------: | :-------: |
| Pops   | `String` | `Integer` |
| Pushes | `String` | `String`  |

### substr

Given a `String` and two `Integer`s on the stack:
`[str, start-idx, end-idx]`

Pushes the substring which starts from `start-idx` and ends at `end-idx`.

Does not change the original `String`.

|        |   S-2    |    S-1    |    S0     |
| :----- | :------: | :-------: | :-------: |
| Pops   | `String` | `Integer` | `Integer` |
| Pushes | `String` | `String`  |           |

### Type Checking Instructions

There's a type check instruction for each [basic datatype](#basic-datatypes)
and [complex datatype](#complex-datatypes). They all work the same:

Push `Integer` 0 if the last value on the stack is not of type `T`.

| Instruction      |                    Checks For                    |
| :--------------- | :----------------------------------------------: |
| `void?`          |                      `Void`                      |
| `integer?`       |                    `Integer`                     |
| `double?`        |                     `Double`                     |
| `number?`        |              `Integer` or `Double`               |
| `fn-ref?`        |               `FunctionReference`                |
| `native-fn-ref?` |            `NativeFunctionReference`             |
| `any-fn-ref?`    | `FunctionReference` or `NativeFunctionReference` |
| `list?`          |                      `List`                      |
| `string?`        |                     `String`                     |
| `custom?`        |                     `Custom`                     |

### Jump Instructions

**NOTE: Check the section about [labels](#labels) to understand why jumps are
generally a bad practice to follow.**

Jump instructions require a [label](#labels) as their argument.

All instructions, except for the unconditional jump `j!`, take one value from
the stack which is either an `Integer` or `Double`. And perform a check on it
which is shown in the table below. If the check is true, a jump to the provided
label is performed.

| Instruction | Checks For |
| :---------- | :--------: |
| `j!`        |     \-     |
| `jz!`       |   `= 0`    |
| `jnz!`      |   `!= 0`   |
| `jgz!`      |   `> 0`    |
| `jgez!`     |   `>= 0`   |
| `jlz!`      |   `< 0`    |
| `jlez!`     |   `<= 0`   |
