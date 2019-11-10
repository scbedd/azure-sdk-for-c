# Actions and Emitters

_Weinberg's Second Law: If builders built buildings the way programmers wrote programs,
then the first woodpecker that came along would destroy civilization._

## Actions

An action is similar to a C# generic type [Action](https://docs.microsoft.com/en-us/dotnet/api/system.action-1?view=netframework-4.8). It contains a pointer to a function `func` and a context `self`.

It's also known as

- an actor,
- a reactor,
- a callback (in some C programs),
- a functor (in C++),
- an observer and a subscriber (in Reactive programming),
- a closure (in some high-level languages),
- a delegate and an event.

```c
typedef struct {
  az_result (*func)(void *, az_span);
  void * self;
} az_span_action;

az_result az_span_action_do(az_span_action const action, az_span const arg) {
  AZ_CONTRACT_ARG_NOT_NULL(action.func);

  return action.func(action.self, arg);
}
```

The [self](https://en.wikipedia.org/wiki/This_%28computer_programming%29) field points on actor data.
This data is also known as

- `this` in C++, C#,
- `context`,
- `me` in VB :-).

## Emitters (push iterators)

An emitter is a `push` iterator. Usually an iterator means a `pull` iterator (it has a function `get`, `next` etc).
A `push` iterator is a function which accepts an action and calls this action for every value in the iterator sequence.

See also https://blogs.msdn.microsoft.com/ericlippert/2009/06/26/iterators-at-the-summer-games/ and
https://blogs.msdn.microsoft.com/ericlippert/2009/07/23/iterator-blocks-part-five-push-vs-pull/.

C has neither GC nor lifetime checks (like Rust https://doc.rust-lang.org/nomicon/lifetimes.html). It also has no
[RAII](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization).
So creating a `pull` iterator is complicated and could be unsafe.
One of the ways to solve the problem is to use a `push` iterator.
It gives some scoped lifetime (a lifetime of a function execution) for local objects, and it's very easy to create
a complicated `push` iterator by using a code flow (a code state machine) instead of a data state machine.

A `push` iterator can have a big set of map/reduce operations similar to a `pull` iterator.
Including concatenation, a flat map (SelectMany), filters, reduce, groupBy, etc.

Also a `push` iterator has some limitations that a `pull` iterator doesn't. It's near impossible
to run two `push` iterators simultaneously. Sometimes it's required for such operations as
`zip merge`, `equal`, `compare`, etc. however serialization/deserialization and reading/writing
to IO don't require comparisons of emitters usually.

It's possible to convert from an iterator to an emitter. The emitter function accepts an iterator and an action, it applies the given action for all items in the iterator. Usually this function is called `for_each`.

An emitter is similar to a cold observable collection in reactive programming (Rx). The main difference is that an emitter is using an action instead of a subscriber.

```c
// (SVO) subject verb object // https://en.wikipedia.org/wiki/Subject–verb–object
az_result az_http_request__emit__span_seq(az_http_request, span_action);
```

As any other iterator type, a `push` iterator is [lazy](https://en.wikipedia.org/wiki/Lazy_evaluation), it means it doesn't produce values until it's asked for. This property can be very useful on platforms with limited resources. 
All iterator transformations, like concatenations and filters are also lazy. 
For example, concatenation of two very long `push` sequences doesn't require memory that can hold these two sequences.

### One Way Street

Both 'pull' and 'push' iterators allow you to write a program as an immutable
[data flow](https://en.wikipedia.org/wiki/Dataflow) which usually doesn't require big intermediate buffers in memory.
Also, the amount of interfaces can be reduced. For example, an interface for a JSON parser should be compatible
with a JSON builder. A JSON parser output is an input for a JSON builder. And a JSON builder output
(usually it's an iterator of byte spans) is an input for a JSON parser.

Because all of these transformations are lazy we can serialize data, stream it to HTTP as JSON, read JSON from HTTP,
and then deserialize it without allocating intermediate buffers.
There are some allocations on stack but these objects are fixed in size and don't depend on size of input/output data
(`O(1)` space).

## Discriminated Unions

C doesn't have discriminated unions. There are two main options to implement discriminated unions, the first one is to use conventions. For example,

```c
typedef enum {
  FOO = 0,
  BAR = 1,
} foo_or_bar_kind;

typedef struct {
  foo_or_bar_kind kind;
  union {
    char const * foo;
    bool bar;
  };
} foo_or_bar;
```

A user must access the `foo` field only if `kind` is `FOO` and access the `bar` field only if `kind` is `BAR`.
Otherwise, we may have an undefined behavior which can lead to all sorts of very bad bugs (dangling pointers,
memory leaks, security issues, etc).

The second option is to use a visitor pattern. Simple example:

```c
typedef void (*foo_action)(int);
typedef void (*bar_action)(bool);
typedef void (*foo_or_bar)(foo_action, bar_action);
```

The second option looks a little bit more complicated, but it eliminates a class of bugs when `kind` is not respected.

The visitor pattern can be represented using actions e.g.

```c
struct {
  str_action foo;
  bool_action bar;
} foo_or_bar_visitor;

// foo_or_bar is an action.
struct {
  az_result (* func)(void *, foo_or_bar_visitor const *);
  void * self;
} foo_or_bar;

// a helper function to safely call the @foo_or_bar action.
az_result foo_or_bar_do(foo_or_bar const self, foo_or_bar_visitor const * p_visitor) {
  AZ_CONTRACT_ARG_NOT_NULL(self.func);

  return self.func(self.self, p_visitor);
}
```

## RAII

As mentioned before, C has no RAII support. It's possible to partly solve the problem by using actions. For example, if we need temporary memory allocation, we can create a function which accepts a size and a span action.

```c
/**
 * The function allocates a temporary mutable span on heap and pass this span to the given action.
 */
az_result az_scoped_alloc(size_t const size, az_mut_span_action const action) {
  uint8_t * const p = malloc(size);
  if (p == NULL) {
    return AZ_ERROR_OUT_OF_MEMORY;
  }
  az_result const result = az_mut_span_action_do(action, ((az_span) { .begin = p, .size = size });
  free(p);
  return result;
}
```