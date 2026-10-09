#include "common.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: sequential <input.csv> <output.json>\n";
        return 1;
    }

    try {
        const auto start = std::chrono::steady_clock::now();
        const auto records = load_records(argv[1]);

        const auto analysis_start = std::chrono::steady_clock::now();
        AnalysisResult result = analyze_range(records, 0, records.size());
        const auto analysis_stop = std::chrono::steady_clock::now();
        detect_anomalies(records, result);
        const double analysis_ms = std::chrono::duration<double, std::milli>(analysis_stop - analysis_start).count();

        const auto stop = std::chrono::steady_clock::now();
        const double elapsed_ms = std::chrono::duration<double, std::milli>(stop - start).count();

        std::ofstream output(argv[2]);
        if (!output) throw std::runtime_error("Cannot write output file: " + std::string(argv[2]));
        output << result_to_json(result, elapsed_ms, analysis_ms, 1);

        std::cout << "Sequential | records=" << records.size()
                  << " | time=" << elapsed_ms << " ms\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
