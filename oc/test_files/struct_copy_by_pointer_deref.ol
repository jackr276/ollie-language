/**
* Author: Jack Robbins
* Test the case where we want to copy a struct by pointer deref. The compiler needs to be able to
* correctly recognize and handle such a case
*/

define struct my_struct {
	x:i32;
	arr:i32[10];
	y:f32;
} as custom_struct;


/**
 * Take and copy a struct from the pointer into the local copy.
 * This should involve no parameter stack memory as the pointer
 * will just be in %rdi
 */
pub fn copy_struct(struct_ptr:custom_struct*) -> i32 {
	//This should function properly
	let copy:custom_struct = *struct_ptr;

	ret copy:arr[2];
}



pub fn main() -> i32 {
	let original:custom_struct = {1, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10], 2.0};
		
	//Should return the internal array at index 2, so we should get 3 with echo $?
	ret @copy_struct(&original);
}
