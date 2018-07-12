IPC Examples
------------

This directory contains inter-process communication examples, written in Rust.
These should start as an entry point to learn about Rust on L4Re, but also about
the IPC mechanisms.

Basic IPC Examples
------------------

These examples use the low-level IPC mechanisms provided by Fiasco, without any
convenience glue. It pretty much looks like C and the only difference might be
the additional documentation.

`single-process`
:   This demonstrates the old-fashioned communication using thread
    capabilities within a L4 task.\
    As a plus, a C implementation for comparison is provided.
`two-task-ipc`
:   This implementation spawns two tasks using a Lua loader script. This script
    also hands in a capability to an IPC gate, used for the communication.

