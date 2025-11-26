
fn tester(i:u32) -> i32{
	ret 1;
}

/**
 * For switch statement testing
 */
pub fn main() -> i32{
	let a:mut i32 = 23;
	
 	declare arr:mut i32[32][3];

	arr[1][2] = 23;

	let b:mut i32 = arr[3][2];

	for(let _:mut i32 = 0; _ < 838; _++){
		a++;
	}

	ret a;
}
