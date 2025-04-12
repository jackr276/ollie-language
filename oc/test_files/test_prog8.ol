
fn tester(i:u32) -> i32{
	ret 1;
}

/**
 * For switch statement testing
 */
fn main() -> i32{
	
	let a:i32 := 23;
	
 	declare arr:i32[32][3];

	arr[1][2] := 23;

	let b:i32 := arr[3][2];

	let _:i32 := 0;

	while(_ < 32) do {
		a++;
		_++;
	}


	ret a;
}
