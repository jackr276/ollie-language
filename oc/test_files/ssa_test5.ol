/**
* SSA Testing
*/

pub fn main() -> i32{
	let mut x:i32 := 33;

	x := 3222;

	if(x <= 32) {
		if(x == 70) {
			x := x + -1;
		}
	} else if(x == 77) 	{
		ret x - 4;
	} else if(x == 3232323) {
		ret x + 4555;
	} else {
		//if(x > 32) {
			x := x - 22222222;
		//}
	}

	x := -1;

	ret 0;
}
