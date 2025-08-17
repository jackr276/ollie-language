/**
* This file will test various cases where the compressed equality should specifically *not* work
*/

fn bad_type() -> i32 {
	let mut x:i32 := 3;
	let mut y:i8 := 2;

	//bad size
	y += 3;
	 
	ret y;
}

fn invalid_op_type_mod() -> i32 {
	let mut x:i32 := 3;
	let mut z:i32 := 32;
	let mut y:i32* := &x;

	//Pointers cannot be modded
	y %= x;

	ret *y;
}

fn invalid_op_divide() -> i32 {
	let mut x:i32 := 3;
	let mut z:i32 := 32;
	let mut y:i32* := &x;

	//Pointers cannot be divided 
	y /= x;

	ret *y;
}

fn invalid_op_shift() -> i32 {
	let mut x:i32 := 3;
	let mut z:i32 := 32;
	let mut y:i32* := &x;

	//Pointers cannot be shifted 
	y <<= x;

	ret *y;
}


fn invalid_op_type() -> i32 {
	let mut x:i32 := 3;
	let mut z:i32 := 32;
	let mut y:i32* := &x;

	//Pointers cannot be multiplied
	y *= x;

	ret *y;
}

fn bad_size() -> i32 {
	let mut x:i32 := 3;
	let mut y:i64 := 222;

	//bad size
	x *= y;

	ret x;
}


pub fn main() -> i32 {
	ret 0;
}
