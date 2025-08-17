/**
* Author: Jack Robbins
* This program is made to negatives
* in both varieties of switch statements
*/

fn negative_c_style(arg:i32) -> i32{
	let mut x:i32 := 32;

	switch(arg){
		case 2:
			x := 32;
		case 1:
			x := -3;
		case 4: 
			break;
		case -3:
			x := 211;
			break;

		case 6:
			x := 22;
			break;

		default:
			x := x - 22;
			break;
	}

	//So it isn't optimized away
	ret x;
}

fn negative_ollie_style(arg:i32) -> i32{
	let mut x:i32 := 32;

	switch(x){
		case -3 -> {
			x := 32;
		}
		case 1 -> {
			x := -3;
		}
		case 4 -> {}
		case 3 -> {
			x := 211;
		}

		case 6 -> {
			x := 22;
		}

		default -> {
			x := x - 22;
		}
	}

	ret x;
}



pub fn main() -> i32 {
	@negative_c_style(3);
	@negative_ollie_style(3);

	ret 0;
}
