/**
* Author: Jack Robbins
* Test the conditional derfencing logic in the CFG constructor
*/

define union my_union {
	x:i32;
	y:i64;
};

pub fn get_to_union1(arr:union my_union*) -> i32 {
	ret arr[1].x;
}

pub fn get_to_union2(arr:union my_union**) -> i32 {
	ret arr[1]->x;
}

pub fn main() -> i32 {
	ret 0;
}
