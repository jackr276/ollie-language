
/**
 * For multi-layer array access
 */
fn main() -> i32{
	let a:i32 := 23;
	let mut b:i32 := 3232;
	let c:i32 := 322322;

	c++;
	
	
	//Declare the array
 	declare arr:i32[32][3];

	//Let's try and populate
	arr[1][2] := 23;

	//Now let's try and access
	b := arr[3][2];
	
	

	
	//Let's do simple access to make sure that it still works
	declare b_arr:i8[3];

	b_arr[2] := 'a';

	let a_temp:i32 := b_arr[2];

	defer a++;
	
	

	ret 0;
}
