/**
 * Author: Jack Robbins
 * Test a case where we want to combine cmp with load operations. We only support doing this for
 * the second operand even though some CMP operations do support this for both
 */

pub fn g_than(x:f32, y:f32*) -> i8 {
	ret x > *y;
}


pub fn g_than_or_eq(x:f32, y:f32*) -> i8 {
	ret x >= *y;
}

pub fn l_than(x:f32, y:f32*) -> i8 {
	ret x < *y;
}


pub fn l_than_or_eq(x:f32, y:f32*) -> i8 {
	ret x <= *y;
}


pub fn eq(x:f32, y:f32*) -> i8 {
	ret x == *y;
}


pub fn not_eq(x:f32, y:f32*) -> i8 {
	ret x != *y;
}


pub fn main() -> i32 {
	let x:f32 = 5.55;
	let y:f32 = 6.67;

	let result:mut i32 = 0;

	result += @g_than(y, &x); //result is now 1
	result += @g_than_or_eq(y, &x); //result is now 2
	result += @l_than(x, &y); //result is now 3
	result += @l_than_or_eq(x, &y); //result is now 4
	result += @eq(x, &y); //result is now 4
	result += @not_eq(x, &y); //result is now 5

	OUNIT: [exit_status = 5]
	ret result;
}
