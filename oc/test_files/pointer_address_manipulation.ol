/**
* Author: Jack Robbins
* 
* Test the ability to use the type system to convert from a float to a pointer 
* in a raw way(bit-level) instead of working through the conversion logic
*/


/**
* Trick the type system into treating this as a float
*/
pub fn negate_float(x:mut f32*) -> void {
	//Trick this into being an int
	let int_bits:i32 = *(<mut i32*>(x));

	let negated:i32 = int_bits ^ (1 << 31);

	//Trick it into being an f32 again
	*f32 = *(<f32*>(&negated));

	ret;
}


pub fn main() -> i32 {
	let x:mut f32 = -1.0;

	@negate_float(&x);
	
	ret 0;
}
