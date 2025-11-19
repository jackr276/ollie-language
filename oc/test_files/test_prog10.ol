
fn tester(i:u32) -> i32{
	ret 1;
}

/**
 * For switch statement testing
 */
pub fn main() -> i32{
	let a:i32 = 23;
	
 	declare mut arr:i32[32][3];

	arr[1][2] = 23;

	let b:i32 = arr[3][2];

	while(a < 32){
		a--;
		break when(@tester(a) == 32);
	}

	ret 0;
}
