# The Pulsar Programming Language Documentation

```lisp
*(*println! val).

*(list-println! list):
  <- list (!length) if 0: .
  (!head) (*println!)
  (list-println!)
  .

*(main args):
  [
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

An important part of the Pulsar programming language is its stack-based nature.

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

Two types of comments exist.

The line comment `//` which comments out an entire line.

And the character comment `;` which can be used to improve code readability
by *visually* separating *logical* expressions.

i.e. `10 -> x; x` is more readable than `10 -> x x`.

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
|         Custom          |    128    |

Support for unsigned integers is planned.

The Custom datatype is mainly intended to be returned by [Native Functions](#native-functions).
So you can have handle-like objects that can keep a pointer to some state.

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

| Symbol |   Name  | Args | Returns |
| :----: | :-----: | :--: | :-----: |
|   %    | Modulus |  2   |    1    |
|   &    | BitAnd  |  2   |    1    |
|  \|    |  BitOr  |  2   |    1    |
|   ~    | BitNot  |  2   |    1    |
|   ^    | BitXor  |  2   |    1    |

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
  if 0 = 0:
    // 'x' within this if statement has the value 10.
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
    if i >= 10:
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
    if i < 10:
      continue
    break
  .
```

### The End Statement

The end statement marks the end of a block if no other Control Flow Statement is encountered.

```lisp
*(*println! val).

*(main args):
  0 -> i
  while i < 10:
    i (*println!)
    i 1 + -> i
  end
  .
```

## Functions

Functions take some arguments from the caller's stack and return some.

A function definition looks like this:

```lisp
*(func-name arg1 arg2 ... argN) -> M:
  // Do Stuff
  .
```

When the function returns, at least M values must be on its stack.
Only the last M values are returned onto the caller's stack.

And of course when called, N values must be on the caller's stack.

Return count (and arrow) can be omitted and 0 is implied.

### Function Calls

Functions are called by `(function)`.

> It's like function definitions but without the `*` in front of
it and the arguments within the parentheses.

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
*(*native-func-name arg1 arg2 ... argN) -> M.
```

Signatures must match so that the right amount of arguments
can be taken and added to the caller's stack.

As always, return count (and arrow) can be omitted to imply 0.

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

The only difference is that if there's a [**control flow statement**](#control-flow-statements)
at the end of a branch, an `else` or `else if` CANNOT be chained.
Though you can just add more `if`s after it (which is the same thing).

The `end` keyword must be specified when no other [**control flow statement**](#control-flow-statements)
is provided to mark the end of the branch.

### Weird cool variants?!?!

All the variants can be described with the following string:

`if [[<lvalue>] [<comp> <lvalue>]]:`

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

The `else` branch can be specified within all `if`s. However, the
"no control flow statement" restriction must be met.

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
datatypes from the [if statement](#if-statement).

If no condition and only an lvalue is provided then it's the same as writing
`while <lvalue> != 0`

If the condition is empty it behaves the same way as `while 1` (a while true loop).

All [Control Flow Statements](#control-flow-statements) are supported.
As with all blocks you must have a control flow statement that marks its end.

### Do Block

The do block is a special skippable block. It can be used to create custom loops
or generate a new scope to hide variables from the other ones.

`continue` loops back to the start:
```lisp
*(*println! val).

*(main args):
  // Has the same behaviour as 'while i < 10'
  0 -> i do:
    i (*println!)
    i 1 + -> i
    if i < 10:
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
    if i < 10:
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

As seen in the [globals example](../examples/globals.pls), you can do
performance-heavy tasks during parsing. That means increasing compile
time to improve runtime efficiency!

That example goes from ~700ms of runtime to ~50us just by pre-computing
the lists it uses (granted that sorting algorithm has its worst case
scenario to deal with, pure nightmare if you ask me).

## Instructions

You can see instructions as *blazing fast* functions!

They are called with `(!instr arg0)`.

Moreover, they map directly to VM instructions. So they don't even have the
overhead of creating a new Frame and Stack.

Internal arguments MUST be Integer Literals and they're completely optional.
Most of the time, the instruction doesn't even need arguments to do its job.

### pop

Pops the last value on the stack.

Accepts `arg0`: if <= 1, 1 value is popped. If > 1 `arg0`, values are popped.

### swap

Swaps the last two values on the stack.

Let `T` and `U` be any type:

|        | S-1 | S0  |
| :----- | :-: | :-: |
| Pops   | `T` | `U` |
| Pushes | `U` | `T` |

### dup

Duplicates the last value on the stack.

Accepts `arg0`: if <= 1, 1 copy is pushed. If > 1 `arg0`, copies are pushed.

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

### icall

The `icall` instruction pops one value from the stack, checks
if it's a `Native/FunctionReference` (raising an error otherwise)
and calls the function pointed by the reference. Of course, the called
function is called like a normal `(function)` call.

See the [references example](../examples/references.pls).

|        |            S-1             |    S0     |
| :----- | :------------------------: | :-------: |
| Pops   | `Native/FunctionReference` | `...Args` |
| Pushes | `...ReturnValues`          |           |

### length

Calculates the length of a `List` or `String` and puts it into the stack.

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
