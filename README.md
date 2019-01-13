# wd

wd is an artifact from an alternative timeline. In this timeline, web browsers
never gained any graphical features, and the web remained centered on text.
Markdown became the dominant language for arranging documents, as HTML was
abandoned shortly after invention.

Somehow though, JavaScript made it through! The APIs available were obviously
very different to what we're used to in our timeline, and quite a bit more
limited in terms of interactivity.

# Building

wd uses a handful of libraries, all of which are readily available.

_Hint: don't bother opening these in `wd`, as they're HTML sites._

1.  [curl](https://curl.haxx.se/) - for fetching documents over http
2.  [openssl](https://www.openssl.org/) - for the https part of libcurl
3.  [termbox](https://github.com/nsf/termbox) - for cross-platform terminal support
4.  [duktape](https://duktape.org/) - for executing javascript

Once you have all those libraries installed, you should just be able to run
`make`, which will build a `wd` binary for you.

# Running

Really easy - if you run `wd` with no arguments, it'll open to a blank page.
If you run it with one argument, it'll open that as its initial URL. There are
no switches or configuration parameters yet.

A fun way to try it out is to run `./wd file://README.md` after building -
this will open this very file!

# JavaScript

JavaScript in this timeline is a bit weird. As markdown doesn't have as rich a
syntactic tree as HTML, there's no easy way to identify individual nodes.

_Hint: if you open this page in `wd`, it will actually execute this code and
display a message at the bottom of your window._

```javascript
/*@wd@*/

set_message(
  "the document object is a `" +
    document.type +
    "` node with " +
    document.children.length +
    " children! your lucky number is: " +
    Math.round(Math.random() * 1000)
);
```

# To-do

I've got quite a bit planned that I'm yet to implement. This list is not in any particular order.

- Media file support using image tags, behaving like elinks
- Self-contained writable "application" documents
- More JavaScript functionality
  - On-page databases accessible to JavaScript using markdown tables
  - Event loop for timers in JavaScript
  - DOM-like traversal
- Nested lists
