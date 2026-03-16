# mgrep

Multithreaded file search tool written in C++17. Recursively searches a directory
tree and distributes files across a thread pool, one worker per logical core.

Faster than grep on large codebases. On small ones the thread startup overhead
wins and grep is faster. Crossover is roughly a few hundred files.

## Build

```bash
make
```

Requires g++ with C++17 and pthreads. Tested on Linux and Windows (MSYS2).

```bash
make debug    # build with AddressSanitizer
make clean
```

## Usage

```
./mgrep [options] <pattern> <path>

  -i             case insensitive
  -E             extended regex (ECMAScript)
  -c             count matches per file, no line output
  -n             hide line numbers
  --no-color     disable ANSI colors
  --ext=.cpp     filter by extension, can repeat
  --depth=N      max directory recursion depth
```

## Examples

```bash
./mgrep "connect(" ./src --ext=.cpp --ext=.h

./mgrep -i -E "TODO|FIXME" ./src --ext=.cpp

./mgrep -c "malloc" ./src

./mgrep --depth=2 "main" .
```

## How it works

The main thread walks the directory tree and pushes each file as a task into a
work queue. Worker threads pull from the queue using a mutex and condition variable.
Results go into a shared vector (separate mutex) and get sorted by filename after
all workers finish so output is always deterministic regardless of which thread
finished first.

Plain string search is the default and is faster. Regex mode uses std::regex
ECMAScript flavor. Binary files get skipped by checking for null bytes in the
first 512 bytes of each file.

## Structure

```
include/
    thread_pool.h   thread pool
    searcher.h      searcher class
    result.h        Match, FileResult, SearchConfig
src/
    main.cpp        CLI, output, timing
    searcher.cpp    directory walk and file search
```

## Benchmark

Tested against GNU grep on the Linux kernel source (~60k .c and .h files).

```bash
time ./mgrep "mutex" /path/to/linux --ext=.c --ext=.h --no-color > /dev/null
time grep -r "mutex" /path/to/linux --include="*.c" --include="*.h" > /dev/null
```

Results on an 8 core machine:

```
mgrep   real 4.1s    user 28.3s
grep    real 19.7s   user 18.9s
```

The user time on mgrep being higher than real confirms the threads are running in
parallel. grep's user and real are close because it is single threaded.

## Notes

- Symlinks are skipped to avoid cycles
- Extension filtering is case insensitive (.CPP matches .cpp)
- Exit code 0 if matches found, 1 if not, same as grep
- Use --no-color when piping output
