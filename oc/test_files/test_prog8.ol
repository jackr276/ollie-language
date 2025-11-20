
fn tester(i:u32) -> i32{
	ret 1;
}

/**
 * For switch statement testing
 */
pub fn main() -> i32{
	let mut a:i32 = 23;
	
 	declare mut arr:i32[32][3];

	arr[1][2] = 23;

	let mut b:i32 = arr[3][2];

	for(let mut _:i32 = 0; _ < 838; _++){
		a++;
	}

	ret a;
}
