/**
* Author: Jack Robbins
* Test a case where we have an ollie switch that only has one member and no default statement
*/


pub fn main() -> i32 {
	let x:mut i32 = 5;

	switch(x){
		case 5 -> {
			x++;
		}
	}

	ret x;
}
