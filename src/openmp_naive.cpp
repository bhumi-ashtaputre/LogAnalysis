#include "common.hpp"

#include <omp.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

static std::vector<LogRecord> load_records(const std::string& path) {
    std::ifstream file(path);
    std::vector<LogRecord> records;
    if (!file) throw std::runtime_error("Cannot open input file: " + path);

    std::string line;
    while (std::getline(file, line)) {
        LogRecord r;
        if (parse_line(line, r))
            records.push_back(std::move(r));
    }
    return records;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: openmp_naive <input.csv> <output.json>\n";
        return 1;
    }

    try {
        auto start = std::chrono::steady_clock::now();
        auto records = load_records(argv[1]);

        AnalysisResult result;

        #pragma omp parallel for
        for (long long i = 0; i < static_cast<long long>(records.size()); ++i) {
            const auto& x = records[static_cast<size_t>(i)];

            // Intentionally poor synchronization: almost every update
            // enters the same critical section. This is for comparison only.
            #pragma omp critical(naive_log_update)
            {
                result.total_requests++;
                if (x.status >= 200 && x.status < 400)
                    result.successful_requests++;
                else
                    result.failed_requests++;

                result.status_counts[x.status]++;
                result.url_counts[x.url]++;
                result.ip_counts[x.ip]++;
                result.url_response_sum[x.url] += x.response_ms;
                result.url_response_count[x.url]++;
                result.response_time_sum += x.response_ms;
            }
        }

        if (result.total_requests > 0)
            result.average_response_ms =
                result.response_time_sum /
                static_cast<double>(result.total_requests);

        sort_and_trim(result);
        detect_anomalies(records, result);

        auto stop = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(stop - start).count();

        std::ofstream out(argv[2]);
        if (!out) throw std::runtime_error("Cannot write output: " + std::string(argv[2]));
        out << result_to_json(result, ms);

        std::cout << "Naive OpenMP: " << records.size()
                  << " valid records, " << ms << " ms\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
