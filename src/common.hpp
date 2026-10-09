#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

struct LogRecord {
    std::string timestamp;
    std::string ip;
    std::string url;
    int status = 0;
    double response_ms = 0.0;
};

struct AnalysisResult {
    long long total_requests = 0;
    long long successful_requests = 0;
    long long failed_requests = 0;
    double response_time_sum = 0.0;
    double average_response_ms = 0.0;

    std::unordered_map<int, long long> status_counts;
    std::unordered_map<std::string, long long> url_counts;
    std::unordered_map<std::string, long long> ip_counts;
    std::unordered_map<std::string, double> url_response_sum;
    std::unordered_map<std::string, long long> url_response_count;

    std::vector<std::string> anomalies;
};

bool parse_line(const std::string& line, LogRecord& record);
std::vector<LogRecord> load_records(const std::string& path);
AnalysisResult analyze_range(const std::vector<LogRecord>& records, std::size_t begin, std::size_t end);
void merge_result(AnalysisResult& target, const AnalysisResult& source);
void detect_anomalies(const std::vector<LogRecord>& records, AnalysisResult& result);
std::string result_to_json(const AnalysisResult& result, double elapsed_ms, double analysis_ms, int threads);
