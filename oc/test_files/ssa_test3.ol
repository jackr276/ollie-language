/**
* SSA Testing
*/

fn main() -> i32{
	let mut x:u32 := 33;

	if( x == 3222) then{
		ret x;
	} else if (x == 11) then{
		x := x + 33;
	} else {
		x--;
	}

	ret 0;
}
