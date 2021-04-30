# Linux Remote Process Injection and Hooking - htop

I was very curious how someone would go about doing this.
There seems to be plenty of tools ([Detours](https://github.com/Microsoft/Detours), [EasyHook](https://easyhook.github.io/), etc) and documentation online on how to hook into running processes in Windows, but I had a hard time finding a concrete example online on how to do it in Linux.
This is the repository I wish I had found 3 days ago.

## Overview

For the sake of this exercise, we are going to assume that `htop` is not open-source.

We want be able to change the uptime value displayed by `htop` while it's running.

For this we need the name and signature of the function responsible.

Fire up `htop` in a terminal window and get it's PID while you're in there.

You can also get the PID with something like this: `ps aux | grep htop | head -n 1`

In another terminal window, start a debug session with the `htop` process using it's PID:

    gdb --pid <pid>

Once attached to the running process, we want to find the function.
Let's start by searching for a function that has the word `uptime` in it:

    (gdb) set case-sensitive off
    (gdb) info functions uptime
    All functions matching regular expression "uptime":

    Non-debugging symbols:
    0x000055caa45ceba0  Platform_getUptime

Looks like `Platform_getUptime` is what we're looking for.
Let's add a breakpoint on that function call so we can see examine it's return value:

    (gdb) break Platform_getUptime
    Breakpoint 1 at 0x55caa45ceba0
    (gdb) continue
    Continuing.

    Breakpoint 1, 0x000055caa45ceba0 in Platform_getUptime ()

Looks like we hit it immediately. Now let's get it's return value:

    (gdb) finish
    Run till exit from #0  0x000055caa45ceba0 in Platform_getUptime ()
    0x000055caa45c654d in ?? ()
    (gdb) print $rax
    $1 = 50689

NOTE: You may need to use the 32-bit $eax register instead. Also this may not work on some CPUs.

It returned the value 50689, which is roughly the number of seconds my machine has been on.

As a proof of concept, let's return the value 1 directly and see what that does.

    (gdb) return (int)1
    Make selected stack frame return now? (y or n) y
    #0  0x000055caa45c654d in ?? ()
    (gdb) continue
    Continuing.

    Breakpoint 1, 0x000055caa45ceba0 in Platform_getUptime ()

We've come back around to our breakpoint and you should now see an uptime of `00:00:01` in `htop`.

At this point we have enough information to write our hook.

### htop_hook.c

Code is short and well documented, but essentially we're using the [subhook](https://github.com/Zeex/subhook) library to hook `htop`'s `Platform_getUptime()` function so that it always returns 0. This results in the htop process always displaying an uptime value or 0.

### inject.py

A very simple script to find the PID of a running process by name and inject a shared library.

We are injecting our hook using the [pyinjector](https://github.com/kmaork/pyinjector) python library which is a wrapper around [kubo/injector](https://github.com/kubo/injector).

## Prerequisites

You'll need a C compiler, cmake, python + pip, and `htop`.

Don't forget to clone recursively to pull in the [subhook](https://github.com/Zeex/subhook) dependency.

    git clone --recursive https://github.com/edouardpoitras/process_injection_example.git

### Building

Should be as simple as:

    cmake .
    make

Also need the python dependency:

    pip install pyinjector

### Kernel ptrace

The Linux kernel makes this possible through the use of [ptrace](https://man7.org/linux/man-pages/man2/ptrace.2.html). Because this is a rather sketchy thing to do usually only performed by debuggers, anti-virus', or malicious actors, the Linux Security Module (yama) will give you a hard time unless your system is configured to allow it.

You can temporarily disable the [ptrace](https://man7.org/linux/man-pages/man2/ptrace.2.html) restriction (until reboot) by modifying the ptrace_scope value:

    echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope

More information can be found [here](https://www.kernel.org/doc/html/latest/admin-guide/LSM/Yama.html).

## Running

Ensure `htop` is running in a terminal on your machine, then inject the hook with the command:

    python inject.py htop libhtop_hook.so

You should notice the `uptime` value change to `00:00:00`.