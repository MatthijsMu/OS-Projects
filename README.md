# OS-Projects
Implementations for the 3 assignments of the course Operating System Concepts,
taught at Radboud University (2023-2024).

# Overview

## 1. Shell
An implementation of a simple shell (like bash), that parses commands and executest these using appropriate system calls. `fork()` is used to create child processes for command chaining. Built-in commands such as cd, chdir, are also handled with system calls. Files are handled with file descriptors and system calls such as `dup2()`. The precise implementation choices and their motivation can be found in the comments and the report. The report also shows why the command chaining does not suffer from the infinite buffer problem and gives a demonstration of how to check this.

## 2. Buffer 
A simple buffer + logger that enables multiple threads to exchange information and log the behaviour of the buffer. It is implemented using some high-level C++ data structures, but still handles the concurrency using locks. Implementation choices can be found in the report and comments.

## 3. Improving caching efficiency
A program's caching efficiency is augmented by changing access patterns, thereby improving its locality of reference. Various implementation choices are discussed in the report. A benchmark is performed using `perf`.
