/**
* Author: Jack Robbins
* This test covers the to/from u64 edge case for converting move memory conversions
* 
* Remember that if we're going from 32 bits to a u64, we do *not* need to add any kind
* of converting move because the zero-extension happens implicitly
*/


//Test going from a 32 bit value into a 64 bit memory region
fn to_u64_memory_pointer(mut ptr:u64*) -> void {
	let mut x:i32 = 33;

	//This will trigger a widening conversion that is implicitly
	//done
	*ptr = x;

	ret;
}


//Test going from a 32 bit value into a 64 bit array
fn to_u64_memory_array(mut arr:u64*) -> void {
	let mut x:i32 = 33;

	//This will trigger a widening conversion that is implicitly
	//done
	arr[32] = x;

	ret;
}


//Test going from a 32 bit down into a u64 value
fn from_32_to_u64_memory_pointer(mut ptr:i32*) -> u64 {
	ret *ptr;
}


//Test going from 32 bit array into a 64 bit value
fn from_32_to_u64_memory_array(mut arr:i32*) -> u64 {
	ret arr[32];
}


//Just a placeholder
pub fn main() -> i32 {
	ret 0;
}
