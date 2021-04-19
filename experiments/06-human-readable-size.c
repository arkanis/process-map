#include <stdio.h>
#include <math.h>

double HumanReadableByteSize(size_t bytes, const char** unit) {
	static const char* units[] = { "Byte", "KiByte", "MiByte", "GiByte", "TiByte", "PiByte", "EiByte", "ZiByte", "YiByte" };
	size_t unitIndex = floor(log2(bytes) / 10);
	if (unitIndex >= sizeof(units) / sizeof(units[0]))
		unitIndex = sizeof(units) / sizeof(units[0]) - 1;
	
	*unit = units[unitIndex];
	return bytes / pow(1024, unitIndex);
}

int main() {
	const char* units[] = { "Byte", "KiByte", "MiByte", "GiByte", "TiByte", "PiByte", "EiByte", "ZiByte", "YiByte" };
	
	size_t s = 33418547llu * 4096llu;
	//size_t s = 4503565268833321llu * 4096llu;
	//size_t s = 59 * 4096;
	//size_t s = 512 * 4096;
	size_t order = floor(log2(s) / 10);
	printf("log2(s): %lf\n", log2(s));
	printf("floor(log2(s) / 10): %zu\n", order);
	
	if (order < sizeof(units) / sizeof(units[0])) {
		double value = s / pow(1024, order);
		printf("%zu bytes → %.2lf %s\n", s, value, units[order]);
	} else {
		printf("just large\n");
	}
	
	const char* unit = NULL;
	double value = HumanReadableByteSize(s, &unit);
	printf("HumanReadableByteSize(): %zu bytes → %.2lf %s\n", s, value, unit);
	
	return 0;
}