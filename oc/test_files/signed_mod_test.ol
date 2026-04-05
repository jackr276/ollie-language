/**
* Author: Jack Robbins
* Test doing modulus with a negative dividend
*/

pub fn mod_non_power_of_2(x:i32) -> i32 {
	ret x % 5;
}

pub fn mod_power_of_2(x:i32) -> i32 {
	ret x % 8;
}


pub fn main() -> i32 {
	//Should return -4 + -7 = -11
	ret @mod_non_power_of_2(-19) + @mod_power_of_2(-15);
}
