/**
* SSA Testing
*/

fn main() -> i32{
	let mut x:u32 := 33;
	defer {
		x++;
	};

	if( x == 3222) then{
		ret x;
	}
	

	x := 323;
	ret 0;
}
