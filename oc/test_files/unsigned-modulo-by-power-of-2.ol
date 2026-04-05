/**
* Author: Jack Robbins
* Test some cases where we do modulo by a power of 2 with
* a specifically unsigned variable
*/


pub fn mod_by_power_2(x:u32) -> u32 {
	let result:mut u32 = 0;
	
	//Test a bunch of individual modulos
	result += x % 2;
	result += x % 4;
	result += x % 8;
	result += x % 16;
	result += x % 32;

	ret result;
}


pub fn main() -> i32 {
	//Should give: 0 + 0 + 0 + 0 + 16 = 16
	ret @mod_by_power_2(16);
}
