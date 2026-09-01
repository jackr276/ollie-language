/**
 * Author: Jack Robbins
 * Test a return by copy fail case where other calls would trample the %rdi register
 * that the return by copy system relies on
 */

define struct my_struct {
	x:mut i32;
	y:mut i32[4];
	z:mut char;
} as custom_struct;


pub fn dummy_fn(x:i32) -> i32 {
	ret x * 2;
}


pub fn ret_by_copy_trampled(x:i32, y:i32) -> struct my_struct {
	//Clobber %rdi
	let one:i32 = @dummy_fn(x);
	let two:i32 = @dummy_fn(y);

	let returned:struct my_struct = {one, [one, two, one, two], 'a'};
	ret returned;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 8]
	ret @ret_by_copy_trampled(5, 4):y[3];
}
