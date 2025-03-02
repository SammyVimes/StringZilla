#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <strstream>
#include <vector>

#include <stringzilla.h>

using strings_t = std::vector<std::string>;
using idx_t = sz_size_t;
using permute_t = std::vector<sz_u64_t>;

#pragma region - C callbacks

static char const *get_start(sz_sequence_t const *array_c, sz_size_t i) {
    strings_t const &array = *reinterpret_cast<strings_t const *>(array_c->handle);
    return array[i].c_str();
}

static sz_size_t get_length(sz_sequence_t const *array_c, sz_size_t i) {
    strings_t const &array = *reinterpret_cast<strings_t const *>(array_c->handle);
    return array[i].size();
}

static int is_less(sz_sequence_t const *array_c, sz_size_t i, sz_size_t j) {
    strings_t const &array = *reinterpret_cast<strings_t const *>(array_c->handle);
    return array[i] < array[j];
}

static int has_under_four_chars(sz_sequence_t const *array_c, sz_size_t i) {
    strings_t const &array = *reinterpret_cast<strings_t const *>(array_c->handle);
    return array[i].size() < 4;
}

#pragma endregion

void populate_from_file( //
    char const *path,
    strings_t &strings,
    std::size_t limit = std::numeric_limits<std::size_t>::max()) {

    std::ifstream f(path, std::ios::in);
    std::string s;
    while (strings.size() < limit && std::getline(f, s, ' ')) strings.push_back(s);
}

void populate_with_test(strings_t &strings) {
    strings.push_back("bbbb");
    strings.push_back("bbbbbb");
    strings.push_back("aac");
    strings.push_back("aa");
    strings.push_back("bb");
    strings.push_back("ab");
    strings.push_back("a");
    strings.push_back("");
    strings.push_back("cccc");
    strings.push_back("ccccccc");
}

constexpr size_t offset_in_word = 0;

inline static idx_t hybrid_sort_cpp(strings_t const &strings, sz_u64_t *order) {

    // What if we take up-to 4 first characters and the index
    for (size_t i = 0; i != strings.size(); ++i)
        std::memcpy((char *)&order[i] + offset_in_word,
                    strings[order[i]].c_str(),
                    std::min(strings[order[i]].size(), 4ul));

    std::sort(order, order + strings.size(), [&](sz_u64_t i, sz_u64_t j) {
        char *i_bytes = (char *)&i;
        char *j_bytes = (char *)&j;
        return *(uint32_t *)(i_bytes + offset_in_word) < *(uint32_t *)(j_bytes + offset_in_word);
    });

    for (size_t i = 0; i != strings.size(); ++i) std::memset((char *)&order[i] + offset_in_word, 0, 4ul);

    std::sort(order, order + strings.size(), [&](sz_u64_t i, sz_u64_t j) { return strings[i] < strings[j]; });

    return strings.size();
}

int hybrid_sort_c_compare_uint32_t(const void *a, const void *b) {
    uint32_t int_a = *((uint32_t *)(((char *)a) + sizeof(sz_size_t) - 4));
    uint32_t int_b = *((uint32_t *)(((char *)b) + sizeof(sz_size_t) - 4));
    return (int_a < int_b) ? -1 : (int_a > int_b);
}

int hybrid_sort_c_compare_strings(void *arg, const void *a, const void *b) {
    sz_sequence_t *sequence = (sz_sequence_t *)arg;
    sz_size_t idx_a = *(sz_size_t *)a;
    sz_size_t idx_b = *(sz_size_t *)b;

    const char *str_a = sequence->get_start(sequence, idx_a);
    const char *str_b = sequence->get_start(sequence, idx_b);
    sz_size_t len_a = sequence->get_length(sequence, idx_a);
    sz_size_t len_b = sequence->get_length(sequence, idx_b);

    int res = strncmp(str_a, str_b, len_a < len_b ? len_a : len_b);
    return res ? res : (int)(len_a - len_b);
}

sz_size_t hybrid_sort_c(sz_sequence_t *sequence) {
    // Copy up to 4 first characters into the 'order' array.
    for (sz_size_t i = 0; i < sequence->count; ++i) {
        const char *str = sequence->get_start(sequence, sequence->order[i]);
        sz_size_t len = sequence->get_length(sequence, sequence->order[i]);
        len = len > 4 ? 4 : len;
        memcpy((char *)&sequence->order[i] + sizeof(sz_size_t) - 4, str, len);
    }

    // Sort based on the first 4 bytes.
    qsort(sequence->order, sequence->count, sizeof(sz_size_t), hybrid_sort_c_compare_uint32_t);

    // Clear the 4 bytes used for the initial sort.
    for (sz_size_t i = 0; i < sequence->count; ++i) {
        memset((char *)&sequence->order[i] + sizeof(sz_size_t) - 4, 0, 4);
    }

    // Sort the full strings.
    qsort_r(sequence->order, sequence->count, sizeof(sz_size_t), sequence, hybrid_sort_c_compare_strings);

    return sequence->count;
}

inline static idx_t hybrid_stable_sort_cpp(strings_t const &strings, sz_u64_t *order) {

    // What if we take up-to 4 first characters and the index
    for (size_t i = 0; i != strings.size(); ++i)
        std::memcpy((char *)&order[i] + offset_in_word,
                    strings[order[i]].c_str(),
                    std::min(strings[order[i]].size(), 4ul));

    std::stable_sort(order, order + strings.size(), [&](sz_u64_t i, sz_u64_t j) {
        char *i_bytes = (char *)&i;
        char *j_bytes = (char *)&j;
        return *(uint32_t *)(i_bytes + offset_in_word) < *(uint32_t *)(j_bytes + offset_in_word);
    });

    for (size_t i = 0; i != strings.size(); ++i) std::memset((char *)&order[i] + offset_in_word, 0, 4ul);

    std::stable_sort(order, order + strings.size(), [&](sz_u64_t i, sz_u64_t j) { return strings[i] < strings[j]; });

    return strings.size();
}

void expect_partitioned_by_length(strings_t const &strings, permute_t const &permute) {
    if (!std::is_partitioned(permute.begin(), permute.end(), [&](size_t i) { return strings[i].size() < 4; }))
        throw std::runtime_error("Partitioning failed!");
}

void expect_sorted(strings_t const &strings, permute_t const &permute) {
    if (!std::is_sorted(permute.begin(), permute.end(), [&](std::size_t i, std::size_t j) {
            return strings[i] < strings[j];
        }))
        throw std::runtime_error("Sorting failed!");
}

void expect_same(permute_t const &permute_base, permute_t const &permute_new) {
    if (!std::equal(permute_base.begin(), permute_base.end(), permute_new.begin()))
        throw std::runtime_error("Permutations differ!");
}

template <typename algo_at>
void bench_permute(char const *name, strings_t &strings, permute_t &permute, algo_at &&algo) {
    namespace stdc = std::chrono;
    using stdcc = stdc::high_resolution_clock;
    constexpr std::size_t iterations = 3;
    stdcc::time_point t1 = stdcc::now();

    // Run multiple iterations
    for (std::size_t i = 0; i != iterations; ++i) {
        std::iota(permute.begin(), permute.end(), 0);
        algo(strings, permute);
    }

    // Measure elapsed time
    stdcc::time_point t2 = stdcc::now();
    double dif = stdc::duration_cast<stdc::nanoseconds>(t2 - t1).count();
    double milisecs = dif / (iterations * 1e6);
    std::printf("Elapsed time is %.2lf miliseconds/iteration for %s.\n", milisecs, name);
}

template <typename algo_at>
void bench_search(char const *name, std::string_view full_text, algo_at &&algo) {
    namespace stdc = std::chrono;
    using stdcc = stdc::high_resolution_clock;
    constexpr std::size_t iterations = 200;
    stdcc::time_point t1 = stdcc::now();

    // Run multiple iterations
    std::size_t bytes_passed = 0;
    for (std::size_t i = 0; i != iterations; ++i) bytes_passed += algo();

    // Measure elapsed time
    stdcc::time_point t2 = stdcc::now();
    double dif = stdc::duration_cast<stdc::nanoseconds>(t2 - t1).count();
    double milisecs = dif / (iterations * 1e6);
    double gbs = bytes_passed / dif;
    std::printf("Elapsed time is %.2lf miliseconds/iteration @ %.2f GB/s for %s.\n", milisecs, gbs, name);
}

int main(int, char const **) {
    std::printf("Hey, Ash!\n");

    strings_t strings;
    populate_from_file("leipzig1M.txt", strings, 1000000);
    std::size_t mean_bytes = 0;
    for (std::string const &str : strings) mean_bytes += str.size();
    mean_bytes /= strings.size();
    std::printf("Parsed the file with %zu words of %zu mean length!\n", strings.size(), mean_bytes);

    std::string full_text;
    full_text.reserve(mean_bytes + strings.size() * 2);
    for (std::string const &str : strings) full_text.append(str), full_text.push_back(' ');

    auto make_random_needle = [](std::string_view full_text) {
        std::size_t length = std::rand() % 6 + 2;
        std::size_t offset = std::rand() % (full_text.size() - length);
        return full_text.substr(offset, length);
    };

    // Search substring
    for (std::size_t needle_len = 1; needle_len <= 0; ++needle_len) {
        std::string needle(needle_len, '\4');
        std::printf("---- Needle length: %zu\n", needle_len);
        bench_search("std::search", full_text, [&]() mutable {
            return std::search(full_text.begin(), full_text.end(), needle.begin(), needle.end()) - full_text.begin();
        });
        bench_search("sz_find_substring_swar", full_text, [&]() mutable {
            sz_string_start_t ptr =
                sz_find_substring_swar(full_text.data(), full_text.size(), needle.data(), needle.size());
            return ptr ? ptr - full_text.data() : full_text.size();
        });
#if defined(__ARM_NEON)
        bench_search("sz_find_substring_neon", full_text, [&]() mutable {
            sz_string_start_t ptr =
                sz_find_substring_neon(full_text.data(), full_text.size(), needle.data(), needle.size());
            return ptr ? ptr - full_text.data() : full_text.size();
        });
#endif
#if defined(__AVX2__)
        bench_search("sz_find_substring_avx2", full_text, [&]() mutable {
            sz_string_start_t ptr =
                sz_find_substring_avx2(full_text.data(), full_text.size(), needle.data(), needle.size());
            return ptr ? ptr - full_text.data() : full_text.size();
        });
#endif
    }

    permute_t permute_base, permute_new;
    permute_base.resize(strings.size());
    permute_new.resize(strings.size());

    // Partitioning
    if (false) {
        std::printf("---- Partitioning:\n");
        bench_permute("std::partition", strings, permute_base, [](strings_t const &strings, permute_t &permute) {
            std::partition(permute.begin(), permute.end(), [&](size_t i) { return strings[i].size() < 4; });
        });
        expect_partitioned_by_length(strings, permute_base);

        bench_permute("std::stable_partition", strings, permute_base, [](strings_t const &strings, permute_t &permute) {
            std::stable_partition(permute.begin(), permute.end(), [&](size_t i) { return strings[i].size() < 4; });
        });
        expect_partitioned_by_length(strings, permute_base);

        bench_permute("sz_partition", strings, permute_new, [](strings_t const &strings, permute_t &permute) {
            sz_sequence_t array;
            array.order = permute.data();
            array.count = strings.size();
            array.handle = &strings;
            sz_partition(&array, &has_under_four_chars);
        });
        expect_partitioned_by_length(strings, permute_new);
        // TODO: expect_same(permute_base, permute_new);
    }

    // Sorting
    if (true) {
        std::printf("---- Sorting:\n");
        bench_permute("std::sort", strings, permute_base, [](strings_t const &strings, permute_t &permute) {
            std::sort(permute.begin(), permute.end(), [&](idx_t i, idx_t j) { return strings[i] < strings[j]; });
        });
        expect_sorted(strings, permute_base);

        bench_permute("sz_sort", strings, permute_new, [](strings_t const &strings, permute_t &permute) {
            sz_sequence_t array;
            array.order = permute.data();
            array.count = strings.size();
            array.handle = &strings;
            array.get_start = get_start;
            array.get_length = get_length;
            sz_sort(&array, nullptr);
        });
        expect_sorted(strings, permute_new);

        bench_permute("sz_sort_introsort", strings, permute_new, [](strings_t const &strings, permute_t &permute) {
            sz_sequence_t array;
            array.order = permute.data();
            array.count = strings.size();
            array.handle = &strings;
            array.get_start = get_start;
            array.get_length = get_length;
            sz_sort_introsort(&array, (sz_sequence_comparator_t)_sz_sort_compare_less_ascii);
        });
        expect_sorted(strings, permute_new);

        bench_permute("hybrid_sort_c", strings, permute_new, [](strings_t const &strings, permute_t &permute) {
            sz_sequence_t array;
            array.order = permute.data();
            array.count = strings.size();
            array.handle = &strings;
            array.get_start = get_start;
            array.get_length = get_length;
            hybrid_sort_c(&array);
        });
        expect_sorted(strings, permute_new);

        bench_permute("hybrid_sort_cpp", strings, permute_new, [](strings_t const &strings, permute_t &permute) {
            hybrid_sort_cpp(strings, permute.data());
        });
        expect_sorted(strings, permute_new);

        std::printf("---- Stable Sorting:\n");
        bench_permute("std::stable_sort", strings, permute_base, [](strings_t const &strings, permute_t &permute) {
            std::stable_sort(permute.begin(), permute.end(), [&](idx_t i, idx_t j) { return strings[i] < strings[j]; });
        });
        expect_sorted(strings, permute_base);

        bench_permute(
            "hybrid_stable_sort_cpp",
            strings,
            permute_base,
            [](strings_t const &strings, permute_t &permute) { hybrid_stable_sort_cpp(strings, permute.data()); });
        expect_sorted(strings, permute_new);
        expect_same(permute_base, permute_new);
    }

    return 0;
}