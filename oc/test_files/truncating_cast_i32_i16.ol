/**
 * Author: Jack Robbins
 * Test the case where we have a truncating cast that goes from an i32 to an i16. This
 * still requires a truncation
 */



pub fn takes_i16(x:i16) -> i16 {
	ret (<u16>x & (0x8000)) >> (typesize(i16) * 8 - 1);
}


pub fn main() -> i32 {
	let x:i32 = -5;

	OUNIT: [exit_status = 1]
	ret @takes_i16(<i16>x);
}
