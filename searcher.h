#pragma once

#include "result.h"
#include "thread_pool.h"

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

class Searcher {
public:
    explicit Searcher(SearchConfig cfg);

    // kicks off the search, blocks until done, returns all results
    std::vector<FileResult> run();

    // stats - call after run()
    size_t filesScanned()  const { return numFilesScanned.load(); }
    size_t filesMatched()  const { return numFilesMatched.load(); }
    size_t totalMatches()  const { return numTotalMatches.load(); }

private:
    void walkDirectory(const fs::path& dir, int depth);
    void searchFile(const fs::path& path);

    // returns empty vector if no matches
    std::vector<Match> findMatches(const std::string& line, int lineNum);

    bool extensionAllowed(const fs::path& p) const;

    SearchConfig config;
    std::unique_ptr<ThreadPool> pool;

    std::vector<FileResult> results;
    std::mutex resultsMutex;  // guards results vec

    // compiled regex, only used if config.useRegex is true
    std::regex pattern;

    std::atomic<size_t> numFilesScanned{0};
    std::atomic<size_t> numFilesMatched{0};
    std::atomic<size_t> numTotalMatches{0};
};
