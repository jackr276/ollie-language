/**
 * Author: Jack Robbins
 * Test a case where we are copying from complex regions
 */


//More of a complex region inside of here
define struct my_struct {
	x:i32;
	b: define mut struct {
		x:mut i32[5];
		y:mut f32;
	   };
	c:char;
};


inline fn build_struct() -> struct my_struct {
	let built:struct my_struct = {5, {[8, 11, 12, 19, 10], 4.44}, 'a'};

	ret built;
}



pub fn main() -> i32 {
	declare arr:mut struct my_struct[5];

	//Copy over
	arr[2] = @build_struct();

	OUNIT: [exit_status = 19]
	ret arr[2]:b:x[3];
}
