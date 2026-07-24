/**
 * Author: Jack Robbins
 * Test our ability to do a truncating cast from a float to a smaller int(i16)
 */


pub fn float_to_small_int(x:f32) -> i16 {
	ret <i16>x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 3]
	ret @float_to_small_int(3.33);
}
