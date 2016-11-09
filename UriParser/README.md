# What is it?
a little C++ library that parses URLs into bite-sized pieces.

## Why another uri parsing library?
Every other url-parsing library I could find was part of a ginormous library which I invariably couldn't build in one or two of my environments.
I got tired of that, so I wrote this.


## But, you're using Standard strings! isn't that super slow?!
When I ran the example on my machine, it parsed something like 700,000 urls in a second. That was fast enough for me. If it's not enough for you, fork away.

## Okay... so how do I use this one?
Take a look at single-example.cpp It should be pretty straightforward if you're into c++. You can also call the extraction methods in your own driver
code. But...why would you do that?

## So, you were lazy and didn't hide the extractors in a class. tsk tsk.
Yes, well, I figure you're probably smart enough to know which functions you want. The whole bit is namespaced, so you won't accidentally run the
extractors even if you happen to be writing code that defines an ExtractProtocol function. I have no love of class-locked methods when functions will
do.


## btw...
and incidentally, this is based on the url spec defined in [RFC 1738](http://www.ietf.org/rfc/rfc1738.txt), which in case anyone else cares, is
decidedly more up to date than the draft-version on w3.org... not that I'm bitter over that hour of wasted whiteboarding or anything.


# License

This code is licensed under the [MIT License](http://opensource.org/licenses/MIT). See `LICENSE.txt`.
