/**
* Author: Jack Robbins
* This file is meant to test all compressed equality operators
*/

fn test_plus() -> i32 {
	let mut x:i32 := 2;
	let mut y:i32 := -3;
	y += x;
	ret y;
}

fn test_minus() -> i32 {
	let mut x:i32 := 2;
	let mut y:i32 := -3;
	y -= x;
	ret y;
}

fn test_times() -> i32 {
	let mut x:i32 := 2;
	let mut y:i32 := -3;
	y *= x;
	ret y;
}

fn test_divide() -> i32 {
	let mut x:i32 := 2;
	let mut y:i32 := -3;
	y /= x;
	ret y;
}

fn test_mod() -> i32 {
	let mut x:i32 := 2;
	let mut y:i32 := -3;
	y %= x;
	ret y;
}

fn test_xor() -> i32 {
	let mut x:i32 := 2;
	let mut y:i32 := -3;
	y ^= x;
	ret y;
}

fn test_and() -> i32 {
	let mut x:i32 := 2;
	let mut y:i32 := -3;
	y &= x;
	ret y;
}

fn test_or() -> i32 {
	let mut x:i32 := 2;
	let mut y:i32 := -3;
	y |= x;
	ret y;
}

fn test_right_shift() -> i32 {
	let mut x:i32 := 2;
	let mut y:i32 := -3;
	y >>= x;
	ret y;
}

fn test_left_shift() -> i32 {
	let mut x:i32 := 2;
	let mut y:i32 := -3;
	y <<= x;
	ret y;
}


fn main() -> i32 {
	ret 0;
}

