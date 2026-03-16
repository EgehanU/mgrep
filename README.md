# mgrep

A multithreaded file search tool, like a simplified grep. Built this to practice
C++17 concurrency ; thread pool, std::filesystem, the works.

Searches recursively through a directory and distributes files across worker threads.
On large codebases it's noticeably faster than single-threaded search, on small ones
the thread overhead actually makes it slower. That's just how threading works.

## Build

```bash
make
```

Needs g++ with C++17 support and pthreads. Tested on Linux and Windows (MSYS2).

```bash
make debug   # enables AddressSanitizer
make clean
```

## Usage

```
./mgrep [options] <pattern> <path>

  -i             case insensitive
  -E             use extended regex (std::regex ECMAScript)
  -c             print match counts only, no lines
  -n             hide line numbers
  --no-color     disable ANSI colors
  --ext=.cpp     only search files with this extension (can repeat)
  --depth=N      max directory recursion depth
```

## Examples

```bash
# search for a function name across all C++ files
./mgrep "connect(" ./src --ext=.cpp --ext=.h

# case insensitive, regex, find all TODOs and FIXMEs
./mgrep -i -E "TODO|FIXME" ./src --ext=.cpp

# count matches per file
./mgrep -c "malloc" ./src

# limit search depth
./mgrep --depth=2 "main" .
```

## How it works

Directory walk runs on the main thread. Each file gets submitted as a task to a
thread pool (one thread per logical core). Workers pull tasks off a shared queue
using a mutex + condition variable. Results land in a shared vector (separate mutex),
then get sorted by filename after all workers finish so output is deterministic.

Plain string search is the default — it's faster. Regex (`-E`) uses std::regex
which is slower but handles complex patterns. Binary files are skipped by checking
for null bytes in the first 512 bytes, same heuristic git uses.

```
include/
    thread_pool.h   — thread pool implementation
    searcher.h      — searcher class declaration
    result.h        — Match, FileResult, SearchConfig structs
src/
    main.cpp        — CLI parsing, output, timing
    searcher.cpp    — directory walk + file search logic
```

## Benchmark

On a small directory grep is faster (thread startup cost dominates).
On large codebases like the Linux kernel (~60k files) mgrep is 3-5x faster.

```bash
time ./mgrep "mutex" /path/to/linux --ext=.c --ext=.h --no-color > /dev/null
time grep -r "mutex" /path/to/linux --include="*.c" --include="*.h" > /dev/null
```

## Notes

- Symlinks are skipped to avoid cycles
- `--ext` matching is case insensitive (.CPP == .cpp)
- Exit code 0 if matches found, 1 if not (same as grep)
- Colors use ANSI escape codes, use `--no-color` if piping output somewhere
