#include "common.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace {

std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

template <typename K, typename V>
std::vector<std::pair<K, V>> sorted_pairs(const std::unordered_map<K, V>& m, std::size_t n) {
    std::vector<std::pair<K, V>> values(m.begin(), m.end());
    std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
    if (values.size() > n) values.resize(n);
    return values;
}

} // namespace

bool parse_line(const std::string& line, LogRecord& r) {
    if (line.empty()) return false;

    std::stringstream ss(line);
    std::string status_text;
    std::string response_text;

    if (!std::getline(ss, r.timestamp, ',')) return false;
    if (!std::getline(ss, r.ip, ',')) return false;
    if (!std::getline(ss, r.url, ',')) return false;
    if (!std::getline(ss, status_text, ',')) return false;
    if (!std::getline(ss, response_text)) return false;

    try {
        r.status = std::stoi(status_text);
        r.response_ms = std::stod(response_text);
    } catch (...) {
        return false;
    }

    return !r.timestamp.empty() && !r.ip.empty() && !r.url.empty();
}

std::vector<LogRecord> load_records(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open input file: " + path);

    std::vector<LogRecord> records;
    std::string line;
    while (std::getline(file, line)) {
        LogRecord record;
        if (parse_line(line, record)) records.push_back(std::move(record));
    }
    return records;
}

AnalysisResult analyze_range(const std::vector<LogRecord>& records, std::size_t begin, std::size_t end) {
    AnalysisResult result;
    end = std::min(end, records.size());

    for (std::size_t i = begin; i < end; ++i) {
        const LogRecord& r = records[i];
        ++result.total_requests;

        if (r.status >= 200 && r.status < 400) ++result.successful_requests;
        else ++result.failed_requests;

        ++result.status_counts[r.status];
        ++result.url_counts[r.url];
        ++result.ip_counts[r.ip];
        result.url_response_sum[r.url] += r.response_ms;
        ++result.url_response_count[r.url];
        result.response_time_sum += r.response_ms;
    }

    if (result.total_requests > 0) {
        result.average_response_ms =
            result.response_time_sum / static_cast<double>(result.total_requests);
    }
    return result;
}

void merge_result(AnalysisResult& target, const AnalysisResult& source) {
    target.total_requests += source.total_requests;
    target.successful_requests += source.successful_requests;
    target.failed_requests += source.failed_requests;
    target.response_time_sum += source.response_time_sum;

    for (const auto& [key, value] : source.status_counts) target.status_counts[key] += value;
    for (const auto& [key, value] : source.url_counts) target.url_counts[key] += value;
    for (const auto& [key, value] : source.ip_counts) target.ip_counts[key] += value;
    for (const auto& [key, value] : source.url_response_sum) target.url_response_sum[key] += value;
    for (const auto& [key, value] : source.url_response_count) target.url_response_count[key] += value;

    if (target.total_requests > 0) {
        target.average_response_ms =
            target.response_time_sum / static_cast<double>(target.total_requests);
    }
}

void detect_anomalies(const std::vector<LogRecord>& records, AnalysisResult& result) {
    result.anomalies.clear();
    if (records.empty()) return;

    // 1. Consecutive server-error burst.
    int consecutive_5xx = 0;
    for (std::size_t i = 0; i < records.size(); ++i) {
        if (records[i].status >= 500 && records[i].status <= 599) {
            ++consecutive_5xx;
            if (consecutive_5xx == 5) {
                result.anomalies.push_back(
                    "Error burst: at least 5 consecutive 5xx responses near record " +
                    std::to_string(i + 1));
            }
        } else {
            consecutive_5xx = 0;
        }
    }

    // 2. Timestamp-based traffic spike. A second is suspicious when its
    // traffic is at least max(10, mean + 3*standard deviation).
    std::unordered_map<std::string, long long> per_second;
    for (const auto& r : records) ++per_second[r.timestamp];

    if (!per_second.empty()) {
        double sum = 0.0;
        for (const auto& [_, count] : per_second) sum += static_cast<double>(count);
        const double mean = sum / static_cast<double>(per_second.size());

        double variance = 0.0;
        for (const auto& [_, count] : per_second) {
            const double d = static_cast<double>(count) - mean;
            variance += d * d;
        }
        variance /= static_cast<double>(per_second.size());
        const double threshold = std::max(10.0, mean + 3.0 * std::sqrt(variance));

        std::vector<std::pair<std::string, long long>> spikes;
        for (const auto& [timestamp, count] : per_second) {
            if (static_cast<double>(count) >= threshold) spikes.push_back({timestamp, count});
        }
        std::sort(spikes.begin(), spikes.end(), [](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second > b.second;
            return a.first < b.first;
        });
        const std::size_t limit = std::min<std::size_t>(spikes.size(), 3);
        for (std::size_t i = 0; i < limit; ++i) {
            result.anomalies.push_back(
                "Traffic spike: " + spikes[i].first + " has " +
                std::to_string(spikes[i].second) + " requests in one second");
        }
    }

    // 3. Brute-force login candidate.
    std::unordered_map<std::string, long long> failed_logins;
    for (const auto& r : records) {
        if (r.url == "/login" && r.status == 401) ++failed_logins[r.ip];
    }
    for (const auto& [ip, count] : failed_logins) {
        if (count >= 5) {
            result.anomalies.push_back(
                "Brute-force candidate: " + ip + " has " +
                std::to_string(count) + " failed login attempts");
        }
    }

    // 4. High-volume IP using a distribution-based threshold.
    if (!result.ip_counts.empty()) {
        double sum = 0.0;
        for (const auto& [_, count] : result.ip_counts) sum += static_cast<double>(count);
        const double mean = sum / static_cast<double>(result.ip_counts.size());
        double variance = 0.0;
        for (const auto& [_, count] : result.ip_counts) {
            const double d = static_cast<double>(count) - mean;
            variance += d * d;
        }
        variance /= static_cast<double>(result.ip_counts.size());
        const double threshold = mean + 2.5 * std::sqrt(variance);

        for (const auto& [ip, count] : result.ip_counts) {
            if (static_cast<double>(count) >= threshold && count >= 20) {
                result.anomalies.push_back(
                    "High-volume IP: " + ip + " generated " +
                    std::to_string(count) + " requests");
            }
        }
    }

    // 5. Slow endpoint.
    for (const auto& [url, count] : result.url_counts) {
        const auto sum_it = result.url_response_sum.find(url);
        const auto count_it = result.url_response_count.find(url);
        if (sum_it != result.url_response_sum.end() &&
            count_it != result.url_response_count.end() && count_it->second > 0) {
            const double avg = sum_it->second / static_cast<double>(count_it->second);
            if (avg >= 1000.0) {
                std::ostringstream message;
                message << std::fixed << std::setprecision(1)
                        << "Slow URL: " << url << " average response " << avg << " ms";
                result.anomalies.push_back(message.str());
            }
        }
    }

    // Remove duplicate messages while preserving deterministic content.
    std::vector<std::string> unique;
    std::unordered_set<std::string> seen;
    for (const auto& message : result.anomalies) {
        if (seen.insert(message).second) unique.push_back(message);
    }
    result.anomalies.swap(unique);
}

std::string result_to_json(const AnalysisResult& result, double elapsed_ms, double analysis_ms, int threads) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    out << "{\n";
    out << "  \"total_requests\": " << result.total_requests << ",\n";
    out << "  \"successful_requests\": " << result.successful_requests << ",\n";
    out << "  \"failed_requests\": " << result.failed_requests << ",\n";
    out << "  \"average_response_ms\": " << result.average_response_ms << ",\n";
    out << "  \"elapsed_ms\": " << elapsed_ms << ",\n";
    out << "  \"analysis_ms\": " << analysis_ms << ",\n";
    out << "  \"threads\": " << threads << ",\n";

    out << "  \"status_counts\": {";
    bool first = true;
    for (const auto& [status, count] : sorted_pairs(result.status_counts, result.status_counts.size())) {
        if (!first) out << ", ";
        first = false;
        out << "\"" << status << "\": " << count;
    }
    out << "},\n";

    const auto urls = sorted_pairs(result.url_counts, 10);
    out << "  \"top_urls\": [";
    first = true;
    for (const auto& [url, count] : urls) {
        if (!first) out << ", ";
        first = false;
        const double avg = result.url_response_sum.at(url) /
                           static_cast<double>(result.url_response_count.at(url));
        out << "{\"url\":\"" << json_escape(url) << "\",\"requests\":"
            << count << ",\"avg_response_ms\":" << avg << "}";
    }
    out << "],\n";

    const auto ips = sorted_pairs(result.ip_counts, 10);
    out << "  \"top_ips\": [";
    first = true;
    for (const auto& [ip, count] : ips) {
        if (!first) out << ", ";
        first = false;
        out << "{\"ip\":\"" << json_escape(ip) << "\",\"requests\":" << count << "}";
    }
    out << "],\n";

    out << "  \"anomalies\": [";
    first = true;
    for (const auto& anomaly : result.anomalies) {
        if (!first) out << ", ";
        first = false;
        out << "\"" << json_escape(anomaly) << "\"";
    }
    out << "]\n";
    out << "}\n";
    return out.str();
}
