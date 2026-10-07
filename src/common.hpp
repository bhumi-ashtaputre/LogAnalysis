#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct LogRecord {
    std::string timestamp;   // YYYY-MM-DD HH:MM:SS
    std::string ip;
    std::string url;
    int status = 0;
    double response_ms = 0.0;
};

struct AnalysisResult {
    long long total_requests = 0;
    long long successful_requests = 0;
    long long failed_requests = 0;
    double average_response_ms = 0.0;
    double response_time_sum = 0.0;

    std::unordered_map<int, long long> status_counts;
    std::unordered_map<std::string, long long> url_counts;
    std::unordered_map<std::string, long long> ip_counts;
    std::unordered_map<std::string, double> url_response_sum;
    std::unordered_map<std::string, long long> url_response_count;
    std::unordered_map<std::string, long long> minute_counts;
    std::unordered_map<std::string, long long> minute_error_counts;
    std::unordered_map<std::string, long long> failed_login_counts;

    std::vector<std::string> anomalies;
};

bool parse_line(const std::string& line, LogRecord& record);
AnalysisResult analyze_range(const std::vector<LogRecord>& records, size_t begin, size_t end);
void merge_result(AnalysisResult& target, const AnalysisResult& source);
void sort_and_trim(AnalysisResult& result, size_t top_n = 10);
void detect_anomalies(const std::vector<LogRecord>& records, AnalysisResult& result);
std::string result_to_json(const AnalysisResult& result, double elapsed_ms);
