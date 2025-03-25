/**
* SSA Testing
*/

#file SSA_TEST1;

fn test_func() -> void{
	let mut x:u32 := 33;

	for(x := 0; x < 3232; ++x) do {
		@test_func();
	}
}

fn tester() -> i32{
	ret -1;
}

fn main() -> i32{
	let mut x:u32 := 33;
	defer x++;

	if( x == 3222) then{
		ret x;
	}
	

	x := 323;

	for(x := 0; x < 3232; ++x) do {
		@tester();
	}

	ret 0;
}
