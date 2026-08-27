/**
 * Author: Jack Robbins
 * Test that we can still use a struct that we've passed by copy after
 * we've passed it
 */


define struct my_struct {
	x:mut i32[5];
	y:mut i64;
	z:mut char;
};


pub fn use_pass_by_copy(param:struct my_struct, x:i32) -> i32 {
	let result:mut i32 = 0;

	for(let i:mut i32 = 0; i < x; i++){
		result += param:x[i];
	}

	ret result + <i32>param:y;
}


pub fn pass_by_copy() -> i32 {
	let original:struct my_struct = {[1, 2, 3, 4, 5], 6, 'a'};

	//Here's where our copy happens - should give back 21
	let result1:i32 = @use_pass_by_copy(original, 5);

	//Let's use it again
	let copy:struct my_struct = original;

	//Make sure they're distinct
	original:x[1] = 7;

	//Should return 21 + 7 + 2 = 30
	ret result1 + original:x[1] + copy:x[1];
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 30]
	ret @pass_by_copy();
}
