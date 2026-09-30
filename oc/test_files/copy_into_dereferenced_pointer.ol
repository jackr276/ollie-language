/**
 * Author: Jack Robbins
 * Test a copy assignment where the LHS is a dereferenced pointer
 */


define struct my_struct {
	x:i32;
	y:i64;
	arr:i32[10];
	c:char;
};


pub fn copy_into_struct(ptr:mut struct my_struct*) -> void {
	let dummy:struct my_struct = {5, 6, [10, 9, 8, 7, 6, 5, 4, 3, 2, 1], 5};

	//Copy assignment
	*ptr = dummy;
}



pub fn main() -> i32 {
	declare result_struct:mut struct my_struct;

	@copy_into_struct(&result_struct);

	OUNIT: [exit_status = 5]
	ret result_struct:arr[5];
}
