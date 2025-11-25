/**
* Author: Jack Robbins
* This test covers the to/from u64 edge case for converting move memory conversions
* 
* Remember that if we're going from 32 bits to a u64, we do *not* need to add any kind
* of converting move because the zero-extension happens implicitly
*/


//Test going from a 32 bit value into a 64 bit memory region
fn to_u64_memory_pointer(ptr:mut u64*) -> void {
	let x:mut i32 = 33;

	//This will trigger a widening conversion that is implicitly
	//done
	*ptr = x;

	ret;
}


//Test going from a 32 bit value into a 64 bit array
fn to_u64_memory_array(arr:mut u64*) -> void {
	let x:mut i32 = 33;

	//This will trigger a widening conversion that is implicitly
	//done
	arr[32] = x;

	ret;
}


//Test going from a 32 bit down into a u64 value
fn from_32_to_u64_memory_pointer(ptr:mut i32*) -> u64 {
	ret *ptr;
}


//Test going from a 32 bit down into a u64 value
fn from_32_to_u64_memory_assignment(ptr:mut i32*) -> u64 {
	let x:mut u64 = *ptr;

	x = x + 33;

	ret x;
}


//Test going from 32 bit array into a 64 bit value
fn from_32_to_u64_memory_array(arr:mut i32*) -> u64 {
	ret arr[32];
}


//Test going from 32 bit array into a 64 bit value with an index
fn from_32_to_u64_memory_array_with_index(arr:mut i32*, idx:i32) -> u64 {
	ret arr[idx];
}


//Just a placeholder
pub fn main() -> i32 {
	ret 0;
}
