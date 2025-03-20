## tosuto 

_the scripting language that aims to be your friend..._
<div style="text-size: 0.3rem; color: lightslategray">...and also a toaster?</div>

Tosuto is designed to be easy to write and extend, as well
as easily interact with C++.

In the future, I want this language to have something special about it, so I'll
add some i18n capabilities to it.

### An example
```c++
@entrypoint
main : argv {
  bread := "bread"

  toaster.(bread)
  log.(bread) // output: toast
}

toaster := [|
  bread = "toast"
  `()` : my* bread* -> bread = my.bread
|]
```

Here's an example program that toasts some bread.

We create a main function, marked with the ``@entrypoint`` attribute. Inside the
function, we define a variable ``bread`` whose value is ``"bread"``, and we
pass it to the overloaded call operator on an object called ``toaster``, which
takes two arguments: an implicitly passed ``my``, as well as ``bread``. Both are
passed by reference, so when we set the argument ``bread`` to ``my.bread``, the
outer variable ``bread`` is set to ``"toast"``.

### Syntax

#### Identifiers
Each source file is lexed in UTF-32, allowing special characters to be used in
identifiers and strings. Currently, alphanumeric characters, ``$``, ``_``, the
top half of extended ASCII, and half-width katakana characters are supported, due
to me being too lazy to manually add any more character ranges.

#### Keywords
| name      | action                                                                                                                            |
|-----------|-----------------------------------------------------------------------------------------------------------------------------------|
| ``next``  | in for loops, this acts like the ``continue`` keyword in other languages.                                                         |
| ``break`` | this breaks out of a loop early.                                                                                                  |
| ``ret``   | this is like the ``return`` keyword in other languages. it stops execution of a function early, and optionally returns a value.   |
| ``false`` | represents the constant ``false``.                                                                                                |
| ``true``  | represents the constant ``true``.                                                                                                 |
| ``nil``   | represents the constant ``nil``.                                                                                                  |
| ``if``    | starts the declaration of an ``if`` statement.                                                                                    |
| ``elif``  | equivalent to what occurs when ``else if`` is typed in other languages.                                                           |
| ``else``  | defines the ``else`` case of an ``if`` statement that gets executed when none of the other branches' conditions evaluate to true. |
| ``for``   | declares a ``for`` loop.                                                                                                          |

#### Expressions
| infix operator           | action                                      |
|--------------------------|---------------------------------------------|
| ``a := b``               | declares a variable ``a`` with value ``b``. |
| ``a (+\|-\|*\|/\|%)= b`` | sets ``a`` to ``a (+\|-\|*\|/\|%) b``       |
| ``a (+\|-\|*\|/\|%) b``  | evaluate ``a (+\|-\|*\|/\|%) b``            |

| unary operator | action                                                                  |
|----------------|-------------------------------------------------------------------------|
| ``+a``         | if ``a`` is a number, return ``a``, else throw                          |
| ``-a``         | if ``a`` is a number, return ``-a``, else throw                         |
| ``a*``         | dereference ``a``, only useful with objects that have member ``*deref`` |
| ``!a``         | if ``a`` is truthy, return ``false``, else return ``true``              |

