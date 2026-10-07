#include "common.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <map>

namespace {
std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

template <typename K, typename V>
std::vector<std::pair<K,V>> sorted_pairs(const std::unordered_map<K,V>& m, size_t n) {
    std::vector<std::pair<K,V>> v(m.begin(), m.end());
    std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
    if (v.size() > n) v.resize(n);
    return v;
}
}

bool parse_line(const std::string& line, LogRecord& r) {
    if (line.empty()) return false;

    std::stringstream ss(line);
    std::string status_str, response_str;

    if (!std::getline(ss, r.timestamp, ',')) return false;
    if (!std::getline(ss, r.ip, ',')) return false;
    if (!std::getline(ss, r.url, ',')) return false;
    if (!std::getline(ss, status_str, ',')) return false;
    if (!std::getline(ss, response_str)) return false;

    try {
        r.status = std::stoi(status_str);
        r.response_ms = std::stod(response_str);
    } catch (...) {
        return false;
    }

    return !r.ip.empty() && !r.url.empty();
}

AnalysisResult analyze_range(const std::vector<LogRecord>& records, size_t begin, size_t end) {
    AnalysisResult r;
    if (end > records.size()) end = records.size();

    for (size_t i = begin; i < end; ++i) {
        const auto& x = records[i];

        ++r.total_requests;

        if (x.status >= 200 && x.status < 400)
            ++r.successful_requests;
        else
            ++r.failed_requests;

        ++r.status_counts[x.status];
        ++r.url_counts[x.url];
        ++r.ip_counts[x.ip];

        r.url_response_sum[x.url] += x.response_ms;
        ++r.url_response_count[x.url];
        r.response_time_sum += x.response_ms;
    }

    if (r.total_requests > 0)
        r.average_response_ms =
            r.response_time_sum / static_cast<double>(r.total_requests);

    return r;
}

void merge_result(AnalysisResult& target, const AnalysisResult& source) {
    target.total_requests += source.total_requests;
    target.successful_requests += source.successful_requests;
    target.failed_requests += source.failed_requests;
    target.response_time_sum += source.response_time_sum;

    for (const auto& [k,v] : source.status_counts)
        target.status_counts[k] += v;
    for (const auto& [k,v] : source.url_counts)
        target.url_counts[k] += v;
    for (const auto& [k,v] : source.ip_counts)
        target.ip_counts[k] += v;
    for (const auto& [k,v] : source.url_response_sum)
        target.url_response_sum[k] += v;
    for (const auto& [k,v] : source.url_response_count)
        target.url_response_count[k] += v;

    if (target.total_requests > 0)
        target.average_response_ms =
            target.response_time_sum / static_cast<double>(target.total_requests);
}

void detect_anomalies(const std::vector<LogRecord>& records, AnalysisResult& result) {
    result.anomalies.clear();
    if (records.empty()) return;

    // 1. Error burst: 5 or more consecutive server errors (5xx).
    int consecutive = 0;
    size_t burst_start = 0;
    for (size_t i = 0; i < records.size(); ++i) {
        if (records[i].status >= 500 && records[i].status <= 599) {
            if (consecutive == 0) burst_start = i;
            ++consecutive;
            if (consecutive == 5) {
                result.anomalies.push_back(
                    "Error burst: at least 5 consecutive 5xx responses starting at record "
                    + std::to_string(burst_start + 1));
            }
        } else {
            consecutive = 0;
        }
    }

    // 2. Traffic spike: a 100-record block with >= 1.8x the average block size.
    const size_t BLOCK = 100;
    const size_t blocks = (records.size() + BLOCK - 1) / BLOCK;
    const double avg_block =
        static_cast<double>(records.size()) / static_cast<double>(blocks);

    for (size_t b = 0; b < blocks; ++b) {
        size_t begin = b * BLOCK;
        size_t end = std::min(records.size(), begin + BLOCK);
        size_t count = end - begin;
        // For the synthetic/time-ordered data, each record represents one
        // event. A "traffic spike" therefore means an unusually dense
        // timestamp interval; repeated timestamps are counted here.
        std::map<std::string, int> per_second;
        for (size_t i = begin; i < end; ++i)
            ++per_second[records[i].timestamp];

        int max_per_second = 0;
        for (const auto& [ts, c] : per_second)
            max_per_second = std::max(max_per_second, c);

        if (static_cast<double>(count) >= 1.8 * avg_block &&
            max_per_second >= 2) {
            result.anomalies.push_back(
                "Traffic spike: unusually dense activity in records "
                + std::to_string(begin + 1) + "-" + std::to_string(end));
        }
    }

    // 3. Brute-force IP: >= 5 failed login attempts.
    std::unordered_map<std::string, int> failed_logins;
    for (const auto& r : records) {
        if (r.url == "/login" && r.status == 401)
            ++failed_logins[r.ip];
    }

    for (const auto& [ip, count] : failed_logins) {
        if (count >= 5) {
            result.anomalies.push_back(
                "Brute-force candidate: " + ip +
                " has " + std::to_string(count) + " failed login attempts");
        }
    }

    // 4. High-volume IP.
    for (const auto& [ip, count] : result.ip_counts) {
        if (count >= 20) {
            result.anomalies.push_back(
                "High-volume IP: " + ip +
                " generated " + std::to_string(count) + " requests");
        }
    }

    // 5. Slow URL.
    for (const auto& [url, count] : result.url_counts) {
        auto it = result.url_response_sum.find(url);
        auto cit = result.url_response_count.find(url);
        if (it != result.url_response_sum.end() &&
            cit != result.url_response_count.end() && cit->second > 0) {
            double avg = it->second / static_cast<double>(cit->second);
            if (avg >= 1000.0) {
                std::ostringstream msg;
                msg << std::fixed << std::setprecision(1)
                    << "Slow URL: " << url
                    << " average response " << avg << " ms";
                result.anomalies.push_back(msg.str());
            }
        }
    }

    // Remove duplicate anomaly messages while retaining deterministic order.
    std::vector<std::string> unique;
    std::unordered_set<std::string> seen;
    for (const auto& x : result.anomalies) {
        if (seen.insert(x).second)
            unique.push_back(x);
    }
    result.anomalies.swap(unique);
}

void sort_and_trim(AnalysisResult& result, size_t top_n) {
    (void)result;
    (void)top_n;
    // Top lists are deterministically sorted while producing JSON.
}

std::string result_to_json(const AnalysisResult& result, double elapsed_ms) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3);

    out << "{\n";
    out << "  \"total_requests\": " << result.total_requests << ",\n";
    out << "  \"successful_requests\": " << result.successful_requests << ",\n";
    out << "  \"failed_requests\": " << result.failed_requests << ",\n";
    out << "  \"average_response_ms\": " << result.average_response_ms << ",\n";
    out << "  \"elapsed_ms\": " << elapsed_ms << ",\n";

    out << "  \"status_counts\": {";
    bool first = true;
    for (const auto& [k,v] : sorted_pairs(result.status_counts, result.status_counts.size())) {
        if (!first) out << ", ";
        first = false;
        out << "\"" << k << "\": " << v;
    }
    out << "},\n";

    auto urls = sorted_pairs(result.url_counts, 10);
    out << "  \"top_urls\": [";
    first = true;
    for (const auto& [url,count] : urls) {
        if (!first) out << ", ";
        first = false;
        double avg = result.url_response_sum.at(url) /
                     static_cast<double>(result.url_response_count.at(url));
        out << "{\"url\":\"" << json_escape(url)
            << "\",\"requests\":" << count
            << ",\"avg_response_ms\":" << avg << "}";
    }
    out << "],\n";

    auto ips = sorted_pairs(result.ip_counts, 10);
    out << "  \"top_ips\": [";
    first = true;
    for (const auto& [ip,count] : ips) {
        if (!first) out << ", ";
        first = false;
        out << "{\"ip\":\"" << json_escape(ip)
            << "\",\"requests\":" << count << "}";
    }
    out << "],\n";

    out << "  \"anomalies\": [";
    first = true;
    for (const auto& a : result.anomalies) {
        if (!first) out << ", ";
        first = false;
        out << "\"" << json_escape(a) << "\"";
    }
    out << "]\n";
    out << "}\n";

    return out.str();
}
