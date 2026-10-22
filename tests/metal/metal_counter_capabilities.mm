// SPDX-License-Identifier: GPL-2.0-or-later
// Read-only device/SDK probe. Counter names are capabilities, not samples,
// GPU/DRAM bytes, or a claim that a legacy counter set works with Metal 4.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>

int main()
{
	@autoreleasepool {
		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		if (!device) { std::fputs("FAIL no Metal device\n", stderr); return 1; }
		NSMutableArray *sets = [NSMutableArray new];
		for (id<MTLCounterSet> set in device.counterSets) {
			NSMutableArray *names = [NSMutableArray new];
			for (id<MTLCounter> counter in set.counters) [names addObject:counter.name];
			[sets addObject:@{@"set":set.name, @"counters":names}];
		}
		NSDictionary *result = @{
			@"device":device.name,
			@"os":NSProcessInfo.processInfo.operatingSystemVersionString,
			@"supports_metal4":@([device supportsFamily:MTLGPUFamilyMetal4]),
			@"legacy_counter_sets":sets,
			@"sdk_metal4_counter_heap_types":@[@"Timestamp"],
			@"sdk_timestamp_entry_bytes":@(sizeof(MTL4TimestampHeapEntry)),
			@"gpu_work_submitted":@NO,
			@"physical_memory_bytes_measured":@NO
		};
		NSError *error = nil;
		NSData *json = [NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingSortedKeys error:&error];
		if (!json) { std::fprintf(stderr,"FAIL %s\n",error.localizedDescription.UTF8String); return 1; }
		std::fwrite(json.bytes,1,json.length,stdout); std::putchar('\n');
	}
}
