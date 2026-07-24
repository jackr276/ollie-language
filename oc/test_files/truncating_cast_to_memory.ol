/**
 * Author: Jack Robbins
 * Test a case where we perform a truncating cast when moving into memory
 */


pub fn truncate_to_memory(x:mut i8*, y:i64) -> void {
	*x = <i8>y;
}



pub fn main() -> i32 {
	let mem_region:mut i8 = 0;
	let x:i64 = 500000;

	@truncate_to_memory(&mem_region, x);

	OUNIT: [exit_status = 255]
	ret mem_region;
}
