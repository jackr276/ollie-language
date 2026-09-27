/**
 * Author: Jack Robbins
 * Test a case where we're reaching two levels deep for pointer access
 */


define struct tester {
	next:mut struct tester*;
	data:mut i32;
};


pub fn nested_pointer_access_test(tester:struct tester*) -> i32 {
	ret tester=>next=>next=>data;
}


pub fn main() -> i32 {
	let level3:mut struct tester = {0, 15};
	let level2:mut struct tester = {&level3, 11};
	let level1:mut struct tester = {&level2, 14};

	OUNIT: [exit_status = 15]
	ret @nested_pointer_access_test(&level1);
}
