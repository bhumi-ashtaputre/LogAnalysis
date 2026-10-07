#include "common.hpp"

#include <mpi.h>
#include <omp.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
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

static std::string serialize_result(const AnalysisResult& r) {
    // Compact, deterministic intermediate representation.
    std::ostringstream s;
    s << r.total_requests << '\n'
      << r.successful_requests << '\n'
      << r.failed_requests << '\n'
      << r.response_time_sum << '\n';

    s << r.status_counts.size() << '\n';
    for (const auto& [k,v] : r.status_counts)
        s << k << '\t' << v << '\n';

    s << r.url_counts.size() << '\n';
    for (const auto& [k,v] : r.url_counts)
        s << k << '\t' << v << '\t'
          << r.url_response_sum.at(k) << '\t'
          << r.url_response_count.at(k) << '\n';

    s << r.ip_counts.size() << '\n';
    for (const auto& [k,v] : r.ip_counts)
        s << k << '\t' << v << '\n';

    s << r.minute_counts.size() << '\n';
    for (const auto& [k,v] : r.minute_counts)
        s << k << '\t' << v << '\n';

    s << r.minute_error_counts.size() << '\n';
    for (const auto& [k,v] : r.minute_error_counts)
        s << k << '\t' << v << '\n';

    s << r.failed_login_counts.size() << '\n';
    for (const auto& [k,v] : r.failed_login_counts)
        s << k << '\t' << v << '\n';

    return s.str();
}

static AnalysisResult deserialize_result(const std::string& data) {
    AnalysisResult r;
    std::istringstream s(data);

    s >> r.total_requests;
    s >> r.successful_requests;
    s >> r.failed_requests;
    s >> r.response_time_sum;
    if (r.total_requests > 0)
        r.average_response_ms =
            r.response_time_sum / static_cast<double>(r.total_requests);

    size_t n;
    s >> n;
    std::string line;
    std::getline(s, line);

    for (size_t i = 0; i < n; ++i) {
        std::getline(s, line);
        std::istringstream row(line);
        int k; long long v;
        row >> k >> v;
        r.status_counts[k] = v;
    }

    s >> n;
    std::getline(s, line);
    for (size_t i = 0; i < n; ++i) {
        std::getline(s, line);
        std::istringstream row(line);
        std::string k;
        long long v, count;
        double sum;
        row >> k >> v >> sum >> count;
        r.url_counts[k] = v;
        r.url_response_sum[k] = sum;
        r.url_response_count[k] = count;
    }

    s >> n;
    std::getline(s, line);
    for (size_t i = 0; i < n; ++i) {
        std::getline(s, line);
        std::istringstream row(line);
        std::string k;
        long long v;
        row >> k >> v;
        r.ip_counts[k] = v;
    }

    s >> n; std::getline(s, line);
    for (size_t i = 0; i < n; ++i) {
        std::getline(s, line); std::istringstream row(line);
        std::string k; long long v; row >> k >> v; r.minute_counts[k] = v;
    }

    s >> n; std::getline(s, line);
    for (size_t i = 0; i < n; ++i) {
        std::getline(s, line); std::istringstream row(line);
        std::string k; long long v; row >> k >> v; r.minute_error_counts[k] = v;
    }

    s >> n; std::getline(s, line);
    for (size_t i = 0; i < n; ++i) {
        std::getline(s, line); std::istringstream row(line);
        std::string k; long long v; row >> k >> v; r.failed_login_counts[k] = v;
    }

    return r;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank = 0, size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 3) {
        if (rank == 0)
            std::cerr << "Usage: hybrid <input.csv> <output.json>\n";
        MPI_Finalize();
        return 1;
    }

    try {
        auto start = std::chrono::steady_clock::now();

        // Each MPI process reads the input independently for simplicity and
        // portability. It then analyzes a disjoint range. This is suitable
        // for demonstrating hybrid parallelism; production systems can use
        // MPI-IO or byte-range partitioning to avoid replicated I/O.
        auto records = load_records(argv[1]);

        size_t begin = records.size() * static_cast<size_t>(rank) /
                       static_cast<size_t>(size);
        size_t end = records.size() * static_cast<size_t>(rank + 1) /
                     static_cast<size_t>(size);

        int threads = omp_get_max_threads();
        std::vector<AnalysisResult> local(threads);

        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            int nt = omp_get_num_threads();

            size_t sub_begin = begin +
                (end - begin) * static_cast<size_t>(tid) /
                static_cast<size_t>(nt);

            size_t sub_end = begin +
                (end - begin) * static_cast<size_t>(tid + 1) /
                static_cast<size_t>(nt);

            local[tid] = analyze_range(records, sub_begin, sub_end);
        }

        AnalysisResult process_result;
        for (const auto& r : local)
            merge_result(process_result, r);

        std::string payload = serialize_result(process_result);
        int payload_size = static_cast<int>(payload.size());

        if (rank == 0) {
            AnalysisResult final_result;
            merge_result(final_result, process_result);

            for (int src = 1; src < size; ++src) {
                int received_size = 0;
                MPI_Recv(&received_size, 1, MPI_INT, src, 0,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                std::string received(static_cast<size_t>(received_size), '\0');
                MPI_Recv(received.data(), received_size, MPI_CHAR, src, 1,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                AnalysisResult part = deserialize_result(received);

                // The average stored in each partial result is weighted
                // correctly by merge_result.
                merge_result(final_result, part);
            }

            sort_and_trim(final_result);
            detect_anomalies(records, final_result);

            auto stop = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(stop - start).count();

            std::ofstream out(argv[2]);
            if (!out) throw std::runtime_error("Cannot write output: " + std::string(argv[2]));
            out << result_to_json(final_result, ms);

            std::cout << "Hybrid MPI+OpenMP: " << records.size()
                      << " valid records, " << size << " MPI processes, "
                      << threads << " max threads/process, "
                      << ms << " ms\n";
        } else {
            MPI_Send(&payload_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
            MPI_Send(payload.data(), payload_size, MPI_CHAR, 0, 1,
                     MPI_COMM_WORLD);
        }

    } catch (const std::exception& e) {
        if (rank == 0)
            std::cerr << "Error: " << e.what() << "\n";
        MPI_Finalize();
        return 1;
    }

    MPI_Finalize();
    return 0;
}
