#include <cstdio>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <climits>

#define CAT_(a, b) a ## b
#define CAT(a, b) CAT_(a, b)
#define VARNAME(Var) CAT(Var, __LINE__)

#define ASSOC_MAX 50
#define ARRAY_SIZE (1 << 25)

double measure(int assoc, unsigned int way_size) {
	static char* array[ARRAY_SIZE];

#define BENCHMARK_ITERATIONS_1 1
#define BENCHMARK_ITERATIONS_2 10000000
#define repeat(n) for(int VARNAME(i) = 0; VARNAME(i) < n; ++VARNAME(i))

	{ //init array
		std::vector<int> permutation(assoc);
		std::iota(permutation.begin(), permutation.end(), 0);
		std::shuffle(permutation.begin(), permutation.end(),
					 std::mt19937(std::random_device()()));
		for (int i = 0; i < assoc; ++i) {
			unsigned int stride = way_size / sizeof(char *);
			int prev = permutation[i];
			int next = permutation[(i + 1) % assoc];
			array[prev * stride] = reinterpret_cast<char *>(&array[next * stride]);
		}
	}

	double elapsed_sum = 0;
	char *ptr = reinterpret_cast<char*>(array);
	repeat(BENCHMARK_ITERATIONS_1) {
		repeat(assoc) //warm cache
			ptr = *reinterpret_cast<char**>(ptr);
		auto start = std::chrono::steady_clock::now();
		repeat(BENCHMARK_ITERATIONS_2)
			ptr = *reinterpret_cast<char**>(ptr);
		if(ptr == nullptr) printf("this will never be printed");
		auto finish = std::chrono::steady_clock::now();
		double elapsed_seconds = std::chrono::duration_cast
		        <std::chrono::duration<double>>(finish - start).count();
		elapsed_sum += elapsed_seconds;
	}
	return elapsed_sum / (double)BENCHMARK_ITERATIONS_1;
#undef BENCHMARK_ITERATIONS_1
#undef BENCHMARK_ITERATIONS_2
#undef repeat
}

void predict_assoc_waysize(int &assoc, int &way_size) {
#define repeat(n) for(int VARNAME(i) = 0; VARNAME(i) < n; ++VARNAME(i))
#define MAX_WAY_SIZE_LOG 14
#define THRESHOLD 1.3

	static double meas[2][2 * ASSOC_MAX];
	int cur_way_size = (1 << MAX_WAY_SIZE_LOG);

	for(int j = 0; j < MAX_WAY_SIZE_LOG; ++j) {
		for(int i = 2; i < 2 * ASSOC_MAX; i += 2) {
			meas[j % 2][i] = measure(i, cur_way_size);
			printf("CURRENT ASSOC = %d; CURRENT WAY SIZE = %d; ELAPSED = %lf;\n",
				   i, cur_way_size, meas[j % 2][i]);
		}

		for(int i = 4; i < ASSOC_MAX; i++) {
			double k1 = meas[(j + 1) % 2][i] / meas[(j + 1) % 2][i - 2];
			double k2 = meas[j % 2][2 * i - 2] / meas[j % 2][2 * i - 4];
			if(k1 > THRESHOLD && k2 > THRESHOLD) {
				assoc = i - 2;
				way_size = cur_way_size * 2;
				return;
			}
		}
		std::copy(std::begin(meas[j % 2]),
				  std::end(meas[j % 2]),
				  std::begin(meas[(j + 1) % 2]));
		cur_way_size >>= 1;
	}
	throw std::runtime_error("assoc not found");
#undef repeat
#undef MAX_WAY_SIZE_LOG
#undef TRESHOLD
}

void measure_cache_line(int &line_size, int cache_size, int assoc) {
#define repeat(n) for(int VARNAME(i) = 0; VARNAME(i) < n; ++VARNAME(i))
#define BENCHMARK_ITERATIONS 10000000
#define MAX_CACHE_LINE_LOG 12
#define THRESHOLD 1.3

	const int way_size = cache_size / assoc;
	static double meas[MAX_CACHE_LINE_LOG];
	static char* array[ARRAY_SIZE];

	for(int i = 1; i < MAX_CACHE_LINE_LOG; ++i) {
		int cur_line_size = (1 << i);

		{ //init array
			std::vector<int> permutation(assoc);
			std::iota(permutation.begin(), permutation.end(), 0);
			std::shuffle(permutation.begin(), permutation.end(),
						 std::mt19937(std::random_device()()));

			for (int j = 0; j < assoc; ++j) {
				int stride = way_size / (int)sizeof(char *);
				int prev = permutation[j];
				int next = permutation[(j + 1) % assoc];
				array[prev * stride] = reinterpret_cast<char *>(&array[next * stride]);
			}
			std::shuffle(permutation.begin(), permutation.end(),
						 std::mt19937(std::random_device()()));

			for (int j = 0; j < assoc; ++j) {
				int stride = way_size / (int)sizeof(char *);
				int shift = cur_line_size / (int)sizeof(char *);
				int prev = permutation[j];
				int next = permutation[(j + 1) % assoc];
				array[shift + (assoc + prev) * stride] =
						reinterpret_cast<char *>(&array[shift + (assoc + next) * stride]);
			}
		}

		char *ptr1 = reinterpret_cast<char*>(array);
		char *ptr2 = reinterpret_cast<char*>(array + cur_line_size / sizeof(char*) + assoc * way_size / sizeof(char*));

		{ //warm cache
			repeat(assoc)
				ptr1 = *reinterpret_cast<char**>(ptr1);
			repeat(assoc)
				ptr2 = *reinterpret_cast<char**>(ptr2);
		}

		auto start = std::chrono::steady_clock::now();
		repeat(BENCHMARK_ITERATIONS) {
			repeat(assoc)
				ptr1 = *reinterpret_cast<char**>(ptr1);
			repeat(assoc)
				ptr2 = *reinterpret_cast<char**>(ptr2);
		}
		auto finish = std::chrono::steady_clock::now();
		double elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(finish - start).count();
		meas[i] = elapsed_seconds;

		if(ptr1 == nullptr) printf("this will never be printed");
		if(ptr2 == nullptr) printf("this will never be printed");

		printf("CURRENT LINE SIZE = %d; ELAPSED = %lf;\n", cur_line_size, meas[i]);
	}

	for(int i = 1; i < MAX_CACHE_LINE_LOG; i++) {
		if(meas[i - 1] / meas[i] > THRESHOLD) {
			line_size = (1 << i);
			return;
		}
	}

#undef repeat
#undef BENCHMARK_ITERATIONS
#undef MAX_WAY_SIZE_LOG
#undef THRESHOLD
}

int main() {
	int assoc, way_size, line_size;
	predict_assoc_waysize(assoc, way_size);
	measure_cache_line(line_size, assoc * way_size, assoc);
	printf("==========================================\n");
	printf("LEVEL1_DCACHE_ASSOC: %d\n", assoc);
	printf("LEVEL1_DCACHE_SIZE: %d\n", assoc * way_size);
	printf("LEVEL1_DCACHE_LINESIZE: %d\n", line_size);
	return 0;
}