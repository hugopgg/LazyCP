# Lazy copy util

- ##### Author:

  Hugo Perreault Gravel

- ##### Description:
This C utility is designed for efficient file copying by employing a lazy copy technique, meaning it only transfers data blocks that are not already present in the destination file. It operates using two processes that communicate through pipes, allowing for a streamlined transfer of data. One process reads from the source file while the other writes to the destination, enabling simultaneous operations that enhance performance. By checking the integrity of existing data and utilizing inter-process communication, the utility minimizes redundant transfers, resulting in faster copy operations and reduced resource usage. This approach is particularly beneficial for large files or backups, optimizing both time and storage efficiency while remaining accessible for users of all skill levels.

## Prerequisites

- C Compiler (gcc)
- Make

## Usage

Compile:

```
make
```

Usage:

```
./lcp [-b TAILLE] SOURCE... DESTINATION
```

Clean:

```
make clean
```