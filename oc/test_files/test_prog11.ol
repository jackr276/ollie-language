
/**
 * For multi-layer array access
 */
pub fn main() -> i32{
	let a:mut i32 = 23;
	let b:mut i32 = 3232;
	let c:mut i32 = 322322;

	c++;
	
	
	//Declare the array
 	declare arr:mut i32[32][3];

	//Let's try and populate
	arr[1][2] = 23;

	//Now let's try and access
	b = arr[3][2];

	//Let's do simple access to make sure that it still works
	declare b_arr:mut i8[3];

	b_arr[2] = 'a';

	let a_temp:i32 = b_arr[2];

	defer {a++;}
	
	

	ret 0;
}
