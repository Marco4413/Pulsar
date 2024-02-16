# The Pulsar Programming Language Documentation

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

*(main) -> 1:
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

## Datatypes

Pulsar is dynamically-typed, any value can be assigned to any local.
And of course all values can be pushed onto the stack.

Though some type checking can be done by operators/statements.
And a runtime error may be raised. No error catching is supported
within the language itself.

|          Type           | Bit Count |
| :---------------------: | :-------: |
|         Integer         |    64     |
|         Double          |    64     |
|    FunctionReference    |    64     |
| NativeFunctionReference |    64     |

Support for unsigned integers and (maybe) strings are planned.

## Mathematical Operators

| Symbol |   Name  | Args | Returns |
| :----: | :-----: | :--: | :-----: |
|   *    |   Star  |  2   |    1    |
|   +    |   Add   |  2   |    1    |
|   -    |   Sub   |  2   |    1    |
|   /    |  Divide |  2   |    1    |
|   %    | Modulus |  2   |    1    |

All these operators are self-explainatory.
They're your usual mathematical operations.
Though you have to keep in mind that we're working with a stack!
So operator priority is not a thing, the last one used is the
last one to be performed!

`Double` is stronger than `Integer`, so `1 1.0 +` turns into `2.0`.

The Modulus operator is the only exception to the rule.
It only accepts `Integer`s.

The Divide and Modulus operator are planned but yet to be implemented.

## Comparison Operators

| Symbol |   Name   | Args | Returns |
| :----: | :------: | :--: | :-----: |
|   =    |  Equals  |  2   |    1    |
|   !=   |  NotEq   |  2   |    1    |
|   <    |   Less   |  2   |    1    |
|   <=   | LessOrEq |  2   |    1    |
|   >    |   More   |  2   |    1    |
|   >=   | MoreOrEq |  2   |    1    |

As of now, these are only allowed within if statements.

## Special Operators

Special operators are the ones that do not just consume N values and
push M onto the stack. Like assignment operators or the 'FullStop' (aka Return) operator.
They modify the state of the execution context and/or are parsed differently to other operators.

### The RightArrow Operator

This operator can be used to create your own locals and/or update
values of already-existing locals. It assigns to the specified local
the last value on the stack and pops it.

```lisp
*(func x y) -> 0:
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
*(func x y) -> 0:
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

### The FullStop/Return Operator

.

### The PushReference Operator

The push reference operator is a very powerful operator. It allows you
to push a reference to a function onto the stack, then you can call it
with the [icall!](#icall) keyword instruction.

`<& (function)`

`(*native)` functions can also be passed to the operator.

The provided function follows the [Function Call](#function-calls) syntax.

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
*(main) -> 1:
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

The only difference is that if there's a return operator at the end
of a branch, an `else` or `else if` CANNOT be chained.
Though you can just add more `if`s after it (which is the same thing).

The `end` keyword must be specified when no return operator is provided
to mark the end of the branch.

### Weird cool variants?!?!

All the variants can be described with the following string:

`if [[<lvalue>] [<comp> <lvalue>]]:`

> Woah, that's A LOT of `if`s.

First of all, if both `lvalue`s (and `comp`) are present the `if` is
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
"no return" restriction must be met.

## Keyword Instructions

You can see keyword instructions as *blazing fast* functions!

They map directly to VM instructions. So they don't even have the
overhead of creating a new Frame and Stack.

### ICall!

The `icall!` instruction pops one value from the stack, checks
if it's a `Native/FunctionReference` (raising an error otherwise)
and calls the function pointed by the reference. Of course, the called
function is called like a normal `(function)` call.

See the [references example](examples/references.pls).
