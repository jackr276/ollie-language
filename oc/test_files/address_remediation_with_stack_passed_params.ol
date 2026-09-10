/**
 * Author: Jack Robbins
 * Test an example where we are doing some more heavy duty address remediation
 * with values in a parameter passed stack
 */


pub fn double_ptr(x:i32*) -> i32 {
	ret *x * 2;
}


pub fn lea_gvn(x:i32, arr:params i32) -> i32 {
	//Make it safe
	if(paramcount(arr) <= x){
		ret 0;
	}

	let result:mut i32 = 5;

	//Stack passed parameter address remediation
	result += @double_ptr(&(arr[x]));
	result += @double_ptr(&(arr[x]));

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 33]
	ret @lea_gvn(5, 8, 7, 9, 1, -1, 7, 3, 8, 0, -11, -121, 15, 0xAE, 8, 5);
}
