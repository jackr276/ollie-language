/**
* SSA Testing
*/

fn main() -> i32{
	let mut x:i32 := 33;
	let mut y:i32 := 3232;

	while(x <= 32) do{
		x := x * 3 + x;
	}

	x := x + 3;
	ret x;
}
