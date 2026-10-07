#include "common.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

static std::vector<LogRecord> load_records(const std::string& path) {
    std::ifstream file(path);
    std::vector<LogRecord> records;
    if (!file) {
        throw std::runtime_error("Cannot open input file: " + path);
    }

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
        std::cerr << "Usage: sequential <input.csv> <output.json>\n";
        return 1;
    }

    try {
        auto start = std::chrono::steady_clock::now();

        auto records = load_records(argv[1]);
        auto result = analyze_range(records, 0, records.size());
        sort_and_trim(result);
        detect_anomalies(records, result);

        auto stop = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(stop - start).count();

        std::ofstream out(argv[2]);
        if (!out) throw std::runtime_error("Cannot write output: " + std::string(argv[2]));
        out << result_to_json(result, ms);

        std::cout << "Sequential: " << records.size()
                  << " valid records, " << ms << " ms\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
