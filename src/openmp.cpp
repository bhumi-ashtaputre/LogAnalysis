#include "common.hpp"

#include <omp.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: openmp <input.csv> <output.json>\n";
        return 1;
    }

    try {
        const auto start = std::chrono::steady_clock::now();
        const auto records = load_records(argv[1]);

        omp_set_dynamic(0);
        const int threads = omp_get_max_threads();
        std::vector<AnalysisResult> local(static_cast<std::size_t>(threads));
        const auto analysis_start = std::chrono::steady_clock::now();

        #pragma omp parallel
        {
            const int tid = omp_get_thread_num();
            const int actual_threads = omp_get_num_threads();
            const std::size_t begin =
                records.size() * static_cast<std::size_t>(tid) /
                static_cast<std::size_t>(actual_threads);
            const std::size_t end =
                records.size() * static_cast<std::size_t>(tid + 1) /
                static_cast<std::size_t>(actual_threads);

            local[static_cast<std::size_t>(tid)] =
                analyze_range(records, begin, end);
        }

        AnalysisResult result;
        for (const auto& partial : local) merge_result(result, partial);
        const auto analysis_stop = std::chrono::steady_clock::now();
        const double analysis_ms = std::chrono::duration<double, std::milli>(analysis_stop - analysis_start).count();
        detect_anomalies(records, result);

        const auto stop = std::chrono::steady_clock::now();
        const double elapsed_ms = std::chrono::duration<double, std::milli>(stop - start).count();

        std::ofstream output(argv[2]);
        if (!output) throw std::runtime_error("Cannot write output file: " + std::string(argv[2]));
        output << result_to_json(result, elapsed_ms, analysis_ms, threads);

        std::cout << "OpenMP | records=" << records.size()
                  << " | threads=" << threads
                  << " | time=" << elapsed_ms << " ms\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
