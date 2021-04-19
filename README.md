process-map
===========

A linux tool to visualize the virtual memory of a process. Thread activity is drawn as small dots jumping around on the
map (at the instructions they're executing). The map also shows public symbols from binaries and shared libraries.

<img src=demo.jpg>

Compiling and how to run
------------------------

To compile just run `make`:

~~~ sh
make
~~~

This will download and compile [raylib][1] as well (using `wget`, replace that command in the Makefile with `curl` if
you don't have `wget` available).

[1]: https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux

~~~ sh
sudo ./map PID
~~~

Where `PID` is the id of the process you want to look at (something like `1856`). Unfortunately the tools needs root
access to inspect the threads via `ptrace()`. Otherwise it has no idea what instructions the threads are executing.

Controls
--------

- Left click & drag: Pan around on the map
- Mouse wheel: Zoom in or out. If you get close enough the public symbols will become visible.
- Click on task in the task list: Only shows dots from that task.

Purpose and what it isn't
-------------------------

The tool is meant to give students an easier and intuitive idea of what a process is. It's a nice gimmick. That's it.

It isn't a profiler or any other serious debugging tool. It doesn't measure where CPU time is spend (a flame chart will
do a much better job). It just looks at each threads instruction pointer every so often and plots that on the map. The
visualization doesn't show if the threads are actually doing work or just sitting on their butts (`poll()` and
`pthread_cond_wait()` are popular meeting places).

So don't take what you see to seriously. :wink:

Status of the code
------------------

The code isn't pretty. Primarily because I just wanted to try out raylib and ptrace(). If you like the idea and want to
take it further you better start from scratch. Or at least do some major refactoring.