#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <chrono>
#include <map>
#include <sstream>
#include <iterator>

using namespace std;
using namespace std::chrono;

const vector<int> ARRAY_SIZES = [] {
    vector<int> sizes;
    for (int i = 100; i <= 3000; i += 100) sizes.push_back(i);
    return sizes;
}();

const int NUM_TESTS_PER_SIZE = 5;
const string CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#%:;^&*()-.";

enum DataType { RANDOM, REVERSE, NEARLY_SORTED };

// Генератор строк
class StringGenerator {
    static mt19937 gen;
    static uniform_int_distribution<> char_dist;
    static uniform_int_distribution<> len_dist;

public:
    static vector<string> generate(int size, DataType type) {
        vector<string> arr(size);
        for (auto& s : arr) {
            s.resize(10 + len_dist(gen) % 191);
            std::generate(s.begin(), s.end(), [&] { return CHARS[char_dist(gen)]; });
        }

        if (type != RANDOM) {
            sort(arr.begin(), arr.end());
            if (type == REVERSE) reverse(arr.begin(), arr.end());
            else for (int i = 0; i < size/20; ++i)
                swap(arr[uniform_int_distribution<>(0, size-1)(gen)], 
                     arr[uniform_int_distribution<>(0, size-1)(gen)]);
        }
        return arr;
    }
};
mt19937 StringGenerator::gen(random_device{}());
uniform_int_distribution<> StringGenerator::char_dist(0, CHARS.size()-1);
uniform_int_distribution<> StringGenerator::len_dist(10, 200);

// Тестер сортировок
class StringSortTester {
    static int comparisons;
    
    // Получение символа по позиции
    static inline char char_at(const string& s, int d) { 
        return d < (int)s.size() ? s[d] : '\0'; 
    }

    // Сравнение строк (1 сравнение = 1 операция)
    static int compare(const string& a, const string& b) {
        comparisons++; // Основное изменение: 1 сравнение на вызов
        int min_len = min(a.size(), b.size());
        for (int i = 0; i < min_len; ++i) {
            if (a[i] != b[i]) return a[i] - b[i];
        }
        return a.size() - b.size();
    }

    // 1. Стандартный QuickSort
    static void standard_quicksort(vector<string>& arr, int l, int r) {
        if (l >= r) return;
        string pivot = arr[l + (r-l)/2];
        int i = l, j = r;
        while (i <= j) {
            while (compare(arr[i], pivot) < 0) ++i;
            while (compare(arr[j], pivot) > 0) --j;
            if (i <= j) swap(arr[i++], arr[j--]);
        }
        standard_quicksort(arr, l, j);
        standard_quicksort(arr, i, r);
    }

    // 2. Стандартный MergeSort
    static void standard_mergesort(vector<string>& arr, int l, int r) {
        if (l >= r) return;
        int m = l + (r - l)/2;
        standard_mergesort(arr, l, m);
        standard_mergesort(arr, m+1, r);

        vector<string> temp;
        int i = l, j = m+1;
        while (i <= m && j <= r) {
            if (compare(arr[i], arr[j]) <= 0) temp.push_back(arr[i++]);
            else temp.push_back(arr[j++]);
        }
        while (i <= m) temp.push_back(arr[i++]);
        while (j <= r) temp.push_back(arr[j++]);
        for (int k = 0; k < temp.size(); k++) arr[l + k] = temp[k];
    }

    // 3. Тернарный QuickSort
    static void ternary_quicksort(vector<string>& arr, int l, int r, int d) {
        if (r - l < 32) {
            for (int i = l+1; i <= r; ++i)
                for (int j = i; j > l && compare_at_d(arr[j], arr[j-1], d) < 0; --j)
                    swap(arr[j], arr[j-1]);
            return;
        }

        char pivot = char_at(arr[l + (r-l)/2], d);
        int lt = l, gt = r, i = l;
        while (i <= gt) {
            char c = char_at(arr[i], d);
            if (c < pivot) swap(arr[lt++], arr[i++]);
            else if (c > pivot) swap(arr[i], arr[gt--]);
            else ++i;
        }

        ternary_quicksort(arr, l, lt-1, d);
        if (pivot != '\0') ternary_quicksort(arr, lt, gt, d+1);
        ternary_quicksort(arr, gt+1, r, d);
    }

    // Сравнение для тернарной сортировки
    static int compare_at_d(const string& a, const string& b, int d) {
        comparisons++; // 1 сравнение на вызов
        char ac = char_at(a, d), bc = char_at(b, d);
        return ac != bc ? ac - bc : a.size() - b.size();
    }

    // 4. MSD Radix
    static void msd_radix(vector<string>& arr, int d, int max_len, bool hybrid) {
        if (arr.size() <= 1 || d >= max_len) return;
        if (hybrid && arr.size() < 74) return ternary_quicksort(arr, 0, arr.size()-1, d);

        map<char, vector<string>> groups;
        vector<string> shorts;
        for (auto& s : arr)
            (s.size() <= d) ? shorts.push_back(s) : groups[s[d]].push_back(s);
        
        arr = move(shorts);
        for (auto& [c, v] : groups) {
            msd_radix(v, d+1, max_len, hybrid);
            arr.insert(arr.end(), v.begin(), v.end());
        }
    }

    // 5. MergeSort с LCP
    static vector<string> lcp_merge(const vector<string>& a, const vector<string>& b) {
        vector<string> res;
        size_t i = 0, j = 0;
        while (i < a.size() && j < b.size()) {
            comparisons++; // 1 сравнение на пару строк
            int lcp_val = 0;
            while (lcp_val < (int)min(a[i].size(), b[j].size()) && 
                   a[i][lcp_val] == b[j][lcp_val]) ++lcp_val;
            
            if (lcp_val == (int)a[i].size() || a[i][lcp_val] < b[j][lcp_val])
                res.push_back(a[i++]);
            else 
                res.push_back(b[j++]);
        }
        res.insert(res.end(), a.begin()+i, a.end());
        res.insert(res.end(), b.begin()+j, b.end());
        return res;
    }

    static vector<string> string_mergesort(const vector<string>& arr) {
        if (arr.size() <= 1) return arr;
        auto mid = arr.begin() + arr.size()/2;
        auto left = string_mergesort(vector(arr.begin(), mid));
        auto right = string_mergesort(vector(mid, arr.end()));
        return lcp_merge(left, right);
    }

public:
    static pair<string, int> test_algorithm(vector<string> arr, const string& algo) {
        comparisons = 0;
        auto start = high_resolution_clock::now();
        
        if (algo == "quicksort") standard_quicksort(arr, 0, arr.size()-1);
        else if (algo == "mergesort") standard_mergesort(arr, 0, arr.size()-1);
        else if (algo == "t_quicksort") ternary_quicksort(arr, 0, arr.size()-1, 0);
        else if (algo == "msd_radix") {
            int max_len = 0;
            for (auto& s : arr) max_len = max(max_len, (int)s.size());
            msd_radix(arr, 0, max_len, false);
        }
        else if (algo == "msd_hybrid") {
            int max_len = 0;
            for (auto& s : arr) max_len = max(max_len, (int)s.size());
            msd_radix(arr, 0, max_len, true);
        }
        else if (algo == "lcp_mergesort") arr = string_mergesort(arr);

        auto end = high_resolution_clock::now();
        double time = duration_cast<microseconds>(end - start).count() / 1000.0;
        stringstream ss;
        ss << fixed << time;
        string time_str = ss.str();
        replace(time_str.begin(), time_str.end(), '.', ',');

        return {time_str, comparisons};
    }
};
int StringSortTester::comparisons = 0;

int main() {
    vector<string> algorithms = {
        "quicksort", "mergesort", "t_quicksort",
        "msd_radix", "msd_hybrid", "lcp_mergesort"
    };

    cout << "Algorithm;Size;DataType;Time(ms);Comparisons\n";
    for (int size : ARRAY_SIZES) {
        for (int type : {RANDOM, REVERSE, NEARLY_SORTED}) {
            auto data = StringGenerator::generate(size, static_cast<DataType>(type));
            
            for (auto& algo : algorithms) {
                double total_time = 0;
                int total_comp = 0;
                
                for (int i = 0; i < NUM_TESTS_PER_SIZE; ++i) {
                    auto result = StringSortTester::test_algorithm(data, algo);
                    string time_str = result.first;
                    replace(time_str.begin(), time_str.end(), ',', '.');
                    total_time += stod(time_str);
                    total_comp += result.second;
                }

                stringstream avg_time;
                avg_time << fixed << total_time/NUM_TESTS_PER_SIZE;
                string avg_time_str = avg_time.str();
                replace(avg_time_str.begin(), avg_time_str.end(), '.', ',');

                cout << algo << ";" << size << ";" << type << ";"
                     << avg_time_str << ";" 
                     << total_comp/NUM_TESTS_PER_SIZE << "\n";
            }
        }
    }
}