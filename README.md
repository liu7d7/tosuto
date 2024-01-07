## ﾄｰｽﾄ (Tōsuto)

Tōsuto is a scripting language designed to be easy to write and extend, as well
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

