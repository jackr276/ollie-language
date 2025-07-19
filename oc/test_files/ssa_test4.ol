/**
* SSA Testing
*/

fn main() -> i32{
	let mut x:u32 := 33;

	x := 3222;

	for(let mut i:u32 := 3; i <= 323; ++i){
		x := 3;
		break when( x == 5);
	}

	ret 0;
}
